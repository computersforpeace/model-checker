#include <stdio.h>
#include <algorithm>
#include <mutex>
#include <new>

#include "model.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "snapshot-interface.h"
#include "common.h"
#include "clockvector.h"
#include "cyclegraph.h"
#include "promise.h"
#include "datarace.h"
#include "threads-model.h"
#include "output.h"

#define INITIAL_THREAD_ID	0

ModelChecker *model;

struct bug_message {
	bug_message(const char *str) {
		const char *fmt = "  [BUG] %s\n";
		msg = (char *)snapshot_malloc(strlen(fmt) + strlen(str));
		sprintf(msg, fmt, str);
	}
	~bug_message() { if (msg) snapshot_free(msg); }

	char *msg;
	void print() { model_print("%s", msg); }

	SNAPSHOTALLOC
};

/**
 * Structure for holding small ModelChecker members that should be snapshotted
 */
struct model_snapshot_members {
	model_snapshot_members() :
		current_action(NULL),
		/* First thread created will have id INITIAL_THREAD_ID */
		next_thread_id(INITIAL_THREAD_ID),
		used_sequence_numbers(0),
		next_backtrack(NULL),
		bugs(),
		stats(),
		failed_promise(false),
		too_many_reads(false),
		bad_synchronization(false),
		asserted(false)
	{ }

	~model_snapshot_members() {
		for (unsigned int i = 0; i < bugs.size(); i++)
			delete bugs[i];
		bugs.clear();
	}

	ModelAction *current_action;
	unsigned int next_thread_id;
	modelclock_t used_sequence_numbers;
	ModelAction *next_backtrack;
	std::vector< bug_message *, SnapshotAlloc<bug_message *> > bugs;
	struct execution_stats stats;
	bool failed_promise;
	bool too_many_reads;
	/** @brief Incorrectly-ordered synchronization was made */
	bool bad_synchronization;
	bool asserted;

	SNAPSHOTALLOC
};

/** @brief Constructor */
ModelChecker::ModelChecker(struct model_params params) :
	/* Initialize default scheduler */
	params(params),
	scheduler(new Scheduler()),
	diverge(NULL),
	earliest_diverge(NULL),
	action_trace(new action_list_t()),
	thread_map(new HashTable<int, Thread *, int>()),
	obj_map(new HashTable<const void *, action_list_t *, uintptr_t, 4>()),
	lock_waiters_map(new HashTable<const void *, action_list_t *, uintptr_t, 4>()),
	condvar_waiters_map(new HashTable<const void *, action_list_t *, uintptr_t, 4>()),
	obj_thrd_map(new HashTable<void *, std::vector<action_list_t> *, uintptr_t, 4 >()),
	promises(new std::vector< Promise *, SnapshotAlloc<Promise *> >()),
	futurevalues(new std::vector< struct PendingFutureValue, SnapshotAlloc<struct PendingFutureValue> >()),
	pending_rel_seqs(new std::vector< struct release_seq *, SnapshotAlloc<struct release_seq *> >()),
	thrd_last_action(new std::vector< ModelAction *, SnapshotAlloc<ModelAction *> >(1)),
	thrd_last_fence_release(new std::vector< ModelAction *, SnapshotAlloc<ModelAction *> >()),
	node_stack(new NodeStack()),
	priv(new struct model_snapshot_members()),
	mo_graph(new CycleGraph())
{
	/* Initialize a model-checker thread, for special ModelActions */
	model_thread = new Thread(get_next_id());
	thread_map->put(id_to_int(model_thread->get_id()), model_thread);
}

/** @brief Destructor */
ModelChecker::~ModelChecker()
{
	for (unsigned int i = 0; i < get_num_threads(); i++)
		delete thread_map->get(i);
	delete thread_map;

	delete obj_thrd_map;
	delete obj_map;
	delete lock_waiters_map;
	delete condvar_waiters_map;
	delete action_trace;

	for (unsigned int i = 0; i < promises->size(); i++)
		delete (*promises)[i];
	delete promises;

	delete pending_rel_seqs;

	delete thrd_last_action;
	delete thrd_last_fence_release;
	delete node_stack;
	delete scheduler;
	delete mo_graph;
	delete priv;
}

static action_list_t * get_safe_ptr_action(HashTable<const void *, action_list_t *, uintptr_t, 4> * hash, void * ptr)
{
	action_list_t *tmp = hash->get(ptr);
	if (tmp == NULL) {
		tmp = new action_list_t();
		hash->put(ptr, tmp);
	}
	return tmp;
}

static std::vector<action_list_t> * get_safe_ptr_vect_action(HashTable<void *, std::vector<action_list_t> *, uintptr_t, 4> * hash, void * ptr)
{
	std::vector<action_list_t> *tmp = hash->get(ptr);
	if (tmp == NULL) {
		tmp = new std::vector<action_list_t>();
		hash->put(ptr, tmp);
	}
	return tmp;
}

/**
 * Restores user program to initial state and resets all model-checker data
 * structures.
 */
void ModelChecker::reset_to_initial_state()
{
	DEBUG("+++ Resetting to initial state +++\n");
	node_stack->reset_execution();

	/* Print all model-checker output before rollback */
	fflush(model_out);

	snapshot_backtrack_before(0);
}

/** @return a thread ID for a new Thread */
thread_id_t ModelChecker::get_next_id()
{
	return priv->next_thread_id++;
}

/** @return the number of user threads created during this execution */
unsigned int ModelChecker::get_num_threads() const
{
	return priv->next_thread_id;
}

/**
 * Must be called from user-thread context (e.g., through the global
 * thread_current() interface)
 *
 * @return The currently executing Thread.
 */
Thread * ModelChecker::get_current_thread() const
{
	return scheduler->get_current_thread();
}

/** @return a sequence number for a new ModelAction */
modelclock_t ModelChecker::get_next_seq_num()
{
	return ++priv->used_sequence_numbers;
}

Node * ModelChecker::get_curr_node() const
{
	return node_stack->get_head();
}

/**
 * @brief Choose the next thread to execute.
 *
 * This function chooses the next thread that should execute. It can force the
 * adjacency of read/write portions of a RMW action, force THREAD_CREATE to be
 * followed by a THREAD_START, or it can enforce execution replay/backtracking.
 * The model-checker may have no preference regarding the next thread (i.e.,
 * when exploring a new execution ordering), in which case this will return
 * NULL.
 * @param curr The current ModelAction. This action might guide the choice of
 * next thread.
 * @return The next thread to run. If the model-checker has no preference, NULL.
 */
Thread * ModelChecker::get_next_thread(ModelAction *curr)
{
	thread_id_t tid;

	if (curr != NULL) {
		/* Do not split atomic actions. */
		if (curr->is_rmwr())
			return thread_current();
		else if (curr->get_type() == THREAD_CREATE)
			return curr->get_thread_operand();
	}

	/* Have we completed exploring the preselected path? */
	if (diverge == NULL)
		return NULL;

	/* Else, we are trying to replay an execution */
	ModelAction *next = node_stack->get_next()->get_action();

	if (next == diverge) {
		if (earliest_diverge == NULL || *diverge < *earliest_diverge)
			earliest_diverge = diverge;

		Node *nextnode = next->get_node();
		Node *prevnode = nextnode->get_parent();
		scheduler->update_sleep_set(prevnode);

		/* Reached divergence point */
		if (nextnode->increment_misc()) {
			/* The next node will try to satisfy a different misc_index values. */
			tid = next->get_tid();
			node_stack->pop_restofstack(2);
		} else if (nextnode->increment_promise()) {
			/* The next node will try to satisfy a different set of promises. */
			tid = next->get_tid();
			node_stack->pop_restofstack(2);
		} else if (nextnode->increment_read_from()) {
			/* The next node will read from a different value. */
			tid = next->get_tid();
			node_stack->pop_restofstack(2);
		} else if (nextnode->increment_future_value()) {
			/* The next node will try to read from a different future value. */
			tid = next->get_tid();
			node_stack->pop_restofstack(2);
		} else if (nextnode->increment_relseq_break()) {
			/* The next node will try to resolve a release sequence differently */
			tid = next->get_tid();
			node_stack->pop_restofstack(2);
		} else {
			ASSERT(prevnode);
			/* Make a different thread execute for next step */
			scheduler->add_sleep(get_thread(next->get_tid()));
			tid = prevnode->get_next_backtrack();
			/* Make sure the backtracked thread isn't sleeping. */
			node_stack->pop_restofstack(1);
			if (diverge == earliest_diverge) {
				earliest_diverge = prevnode->get_action();
			}
		}
		/* The correct sleep set is in the parent node. */
		execute_sleep_set();

		DEBUG("*** Divergence point ***\n");

		diverge = NULL;
	} else {
		tid = next->get_tid();
	}
	DEBUG("*** ModelChecker chose next thread = %d ***\n", id_to_int(tid));
	ASSERT(tid != THREAD_ID_T_NONE);
	return thread_map->get(id_to_int(tid));
}

/**
 * We need to know what the next actions of all threads in the sleep
 * set will be.  This method computes them and stores the actions at
 * the corresponding thread object's pending action.
 */

void ModelChecker::execute_sleep_set()
{
	for (unsigned int i = 0; i < get_num_threads(); i++) {
		thread_id_t tid = int_to_id(i);
		Thread *thr = get_thread(tid);
		if (scheduler->is_sleep_set(thr) && thr->get_pending() == NULL) {
			thr->set_state(THREAD_RUNNING);
			scheduler->next_thread(thr);
			Thread::swap(&system_context, thr);
			priv->current_action->set_sleep_flag();
			thr->set_pending(priv->current_action);
		}
	}
}

void ModelChecker::wake_up_sleeping_actions(ModelAction *curr)
{
	for (unsigned int i = 0; i < get_num_threads(); i++) {
		Thread *thr = get_thread(int_to_id(i));
		if (scheduler->is_sleep_set(thr)) {
			ModelAction *pending_act = thr->get_pending();
			if ((!curr->is_rmwr()) && pending_act->could_synchronize_with(curr))
				//Remove this thread from sleep set
				scheduler->remove_sleep(thr);
		}
	}
}

/** @brief Alert the model-checker that an incorrectly-ordered
 * synchronization was made */
void ModelChecker::set_bad_synchronization()
{
	priv->bad_synchronization = true;
}

bool ModelChecker::has_asserted() const
{
	return priv->asserted;
}

void ModelChecker::set_assert()
{
	priv->asserted = true;
}

/**
 * Check if we are in a deadlock. Should only be called at the end of an
 * execution, although it should not give false positives in the middle of an
 * execution (there should be some ENABLED thread).
 *
 * @return True if program is in a deadlock; false otherwise
 */
bool ModelChecker::is_deadlocked() const
{
	bool blocking_threads = false;
	for (unsigned int i = 0; i < get_num_threads(); i++) {
		thread_id_t tid = int_to_id(i);
		if (is_enabled(tid))
			return false;
		Thread *t = get_thread(tid);
		if (!t->is_model_thread() && t->get_pending())
			blocking_threads = true;
	}
	return blocking_threads;
}

/**
 * Check if this is a complete execution. That is, have all thread completed
 * execution (rather than exiting because sleep sets have forced a redundant
 * execution).
 *
 * @return True if the execution is complete.
 */
bool ModelChecker::is_complete_execution() const
{
	for (unsigned int i = 0; i < get_num_threads(); i++)
		if (is_enabled(int_to_id(i)))
			return false;
	return true;
}

/**
 * @brief Assert a bug in the executing program.
 *
 * Use this function to assert any sort of bug in the user program. If the
 * current trace is feasible (actually, a prefix of some feasible execution),
 * then this execution will be aborted, printing the appropriate message. If
 * the current trace is not yet feasible, the error message will be stashed and
 * printed if the execution ever becomes feasible.
 *
 * @param msg Descriptive message for the bug (do not include newline char)
 * @return True if bug is immediately-feasible
 */
bool ModelChecker::assert_bug(const char *msg)
{
	priv->bugs.push_back(new bug_message(msg));

	if (isfeasibleprefix()) {
		set_assert();
		return true;
	}
	return false;
}

/**
 * @brief Assert a bug in the executing program, asserted by a user thread
 * @see ModelChecker::assert_bug
 * @param msg Descriptive message for the bug (do not include newline char)
 */
void ModelChecker::assert_user_bug(const char *msg)
{
	/* If feasible bug, bail out now */
	if (assert_bug(msg))
		switch_to_master(NULL);
}

/** @return True, if any bugs have been reported for this execution */
bool ModelChecker::have_bug_reports() const
{
	return priv->bugs.size() != 0;
}

/** @brief Print bug report listing for this execution (if any bugs exist) */
void ModelChecker::print_bugs() const
{
	if (have_bug_reports()) {
		model_print("Bug report: %zu bug%s detected\n",
				priv->bugs.size(),
				priv->bugs.size() > 1 ? "s" : "");
		for (unsigned int i = 0; i < priv->bugs.size(); i++)
			priv->bugs[i]->print();
	}
}

/**
 * @brief Record end-of-execution stats
 *
 * Must be run when exiting an execution. Records various stats.
 * @see struct execution_stats
 */
void ModelChecker::record_stats()
{
	stats.num_total++;
	if (!isfeasibleprefix())
		stats.num_infeasible++;
	else if (have_bug_reports())
		stats.num_buggy_executions++;
	else if (is_complete_execution())
		stats.num_complete++;
	else
		stats.num_redundant++;
}

/** @brief Print execution stats */
void ModelChecker::print_stats() const
{
	model_print("Number of complete, bug-free executions: %d\n", stats.num_complete);
	model_print("Number of redundant executions: %d\n", stats.num_redundant);
	model_print("Number of buggy executions: %d\n", stats.num_buggy_executions);
	model_print("Number of infeasible executions: %d\n", stats.num_infeasible);
	model_print("Total executions: %d\n", stats.num_total);
	model_print("Total nodes created: %d\n", node_stack->get_total_nodes());
}

/**
 * @brief End-of-exeuction print
 * @param printbugs Should any existing bugs be printed?
 */
void ModelChecker::print_execution(bool printbugs) const
{
	print_program_output();

	if (DBG_ENABLED() || params.verbose) {
		model_print("Earliest divergence point since last feasible execution:\n");
		if (earliest_diverge)
			earliest_diverge->print();
		else
			model_print("(Not set)\n");

		model_print("\n");
		print_stats();
	}

	/* Don't print invalid bugs */
	if (printbugs)
		print_bugs();

	model_print("\n");
	print_summary();
}

/**
 * Queries the model-checker for more executions to explore and, if one
 * exists, resets the model-checker state to execute a new execution.
 *
 * @return If there are more executions to explore, return true. Otherwise,
 * return false.
 */
bool ModelChecker::next_execution()
{
	DBG();
	/* Is this execution a feasible execution that's worth bug-checking? */
	bool complete = isfeasibleprefix() && (is_complete_execution() ||
			have_bug_reports());

	/* End-of-execution bug checks */
	if (complete) {
		if (is_deadlocked())
			assert_bug("Deadlock detected");

		checkDataRaces();
	}

	record_stats();

	/* Output */
	if (DBG_ENABLED() || params.verbose || (complete && have_bug_reports()))
		print_execution(complete);
	else
		clear_program_output();

	if (complete)
		earliest_diverge = NULL;

	if ((diverge = get_next_backtrack()) == NULL)
		return false;

	if (DBG_ENABLED()) {
		model_print("Next execution will diverge at:\n");
		diverge->print();
	}

	reset_to_initial_state();
	return true;
}

ModelAction * ModelChecker::get_last_conflict(ModelAction *act)
{
	switch (act->get_type()) {
	case ATOMIC_FENCE:
	case ATOMIC_READ:
	case ATOMIC_WRITE:
	case ATOMIC_RMW: {
		/* Optimization: relaxed operations don't need backtracking */
		if (act->is_relaxed())
			return NULL;
		/* linear search: from most recent to oldest */
		action_list_t *list = get_safe_ptr_action(obj_map, act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (prev->could_synchronize_with(act))
				return prev;
		}
		break;
	}
	case ATOMIC_LOCK:
	case ATOMIC_TRYLOCK: {
		/* linear search: from most recent to oldest */
		action_list_t *list = get_safe_ptr_action(obj_map, act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (act->is_conflicting_lock(prev))
				return prev;
		}
		break;
	}
	case ATOMIC_UNLOCK: {
		/* linear search: from most recent to oldest */
		action_list_t *list = get_safe_ptr_action(obj_map, act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (!act->same_thread(prev) && prev->is_failed_trylock())
				return prev;
		}
		break;
	}
	case ATOMIC_WAIT: {
		/* linear search: from most recent to oldest */
		action_list_t *list = get_safe_ptr_action(obj_map, act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (!act->same_thread(prev) && prev->is_failed_trylock())
				return prev;
			if (!act->same_thread(prev) && prev->is_notify())
				return prev;
		}
		break;
	}

	case ATOMIC_NOTIFY_ALL:
	case ATOMIC_NOTIFY_ONE: {
		/* linear search: from most recent to oldest */
		action_list_t *list = get_safe_ptr_action(obj_map, act->get_location());
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *prev = *rit;
			if (!act->same_thread(prev) && prev->is_wait())
				return prev;
		}
		break;
	}
	default:
		break;
	}
	return NULL;
}

/** This method finds backtracking points where we should try to
 * reorder the parameter ModelAction against.
 *
 * @param the ModelAction to find backtracking points for.
 */
void ModelChecker::set_backtracking(ModelAction *act)
{
	Thread *t = get_thread(act);
	ModelAction *prev = get_last_conflict(act);
	if (prev == NULL)
		return;

	Node *node = prev->get_node()->get_parent();

	int low_tid, high_tid;
	if (node->is_enabled(t)) {
		low_tid = id_to_int(act->get_tid());
		high_tid = low_tid + 1;
	} else {
		low_tid = 0;
		high_tid = get_num_threads();
	}

	for (int i = low_tid; i < high_tid; i++) {
		thread_id_t tid = int_to_id(i);

		/* Make sure this thread can be enabled here. */
		if (i >= node->get_num_threads())
			break;

		/* Don't backtrack into a point where the thread is disabled or sleeping. */
		if (node->enabled_status(tid) != THREAD_ENABLED)
			continue;

		/* Check if this has been explored already */
		if (node->has_been_explored(tid))
			continue;

		/* See if fairness allows */
		if (model->params.fairwindow != 0 && !node->has_priority(tid)) {
			bool unfair = false;
			for (int t = 0; t < node->get_num_threads(); t++) {
				thread_id_t tother = int_to_id(t);
				if (node->is_enabled(tother) && node->has_priority(tother)) {
					unfair = true;
					break;
				}
			}
			if (unfair)
				continue;
		}
		/* Cache the latest backtracking point */
		set_latest_backtrack(prev);

		/* If this is a new backtracking point, mark the tree */
		if (!node->set_backtrack(tid))
			continue;
		DEBUG("Setting backtrack: conflict = %d, instead tid = %d\n",
					id_to_int(prev->get_tid()),
					id_to_int(t->get_id()));
		if (DBG_ENABLED()) {
			prev->print();
			act->print();
		}
	}
}

/**
 * @brief Cache the a backtracking point as the "most recent", if eligible
 *
 * Note that this does not prepare the NodeStack for this backtracking
 * operation, it only caches the action on a per-execution basis
 *
 * @param act The operation at which we should explore a different next action
 * (i.e., backtracking point)
 * @return True, if this action is now the most recent backtracking point;
 * false otherwise
 */
bool ModelChecker::set_latest_backtrack(ModelAction *act)
{
	if (!priv->next_backtrack || *act > *priv->next_backtrack) {
		priv->next_backtrack = act;
		return true;
	}
	return false;
}

/**
 * Returns last backtracking point. The model checker will explore a different
 * path for this point in the next execution.
 * @return The ModelAction at which the next execution should diverge.
 */
ModelAction * ModelChecker::get_next_backtrack()
{
	ModelAction *next = priv->next_backtrack;
	priv->next_backtrack = NULL;
	return next;
}

/**
 * Processes a read or rmw model action.
 * @param curr is the read model action to process.
 * @param second_part_of_rmw is boolean that is true is this is the second action of a rmw.
 * @return True if processing this read updates the mo_graph.
 */
bool ModelChecker::process_read(ModelAction *curr, bool second_part_of_rmw)
{
	uint64_t value = VALUE_NONE;
	bool updated = false;
	while (true) {
		const ModelAction *reads_from = curr->get_node()->get_read_from();
		if (reads_from != NULL) {
			mo_graph->startChanges();

			value = reads_from->get_value();
			bool r_status = false;

			if (!second_part_of_rmw) {
				check_recency(curr, reads_from);
				r_status = r_modification_order(curr, reads_from);
			}


			if (!second_part_of_rmw && is_infeasible() && (curr->get_node()->increment_read_from() || curr->get_node()->increment_future_value())) {
				mo_graph->rollbackChanges();
				priv->too_many_reads = false;
				continue;
			}

			read_from(curr, reads_from);
			mo_graph->commitChanges();
			mo_check_promises(curr->get_tid(), reads_from);

			updated |= r_status;
		} else if (!second_part_of_rmw) {
			/* Read from future value */
			value = curr->get_node()->get_future_value();
			modelclock_t expiration = curr->get_node()->get_future_value_expiration();
			curr->set_read_from(NULL);
			Promise *valuepromise = new Promise(curr, value, expiration);
			promises->push_back(valuepromise);
		}
		get_thread(curr)->set_return_value(value);
		return updated;
	}
}

/**
 * Processes a lock, trylock, or unlock model action.  @param curr is
 * the read model action to process.
 *
 * The try lock operation checks whether the lock is taken.  If not,
 * it falls to the normal lock operation case.  If so, it returns
 * fail.
 *
 * The lock operation has already been checked that it is enabled, so
 * it just grabs the lock and synchronizes with the previous unlock.
 *
 * The unlock operation has to re-enable all of the threads that are
 * waiting on the lock.
 *
 * @return True if synchronization was updated; false otherwise
 */
bool ModelChecker::process_mutex(ModelAction *curr)
{
	std::mutex *mutex = NULL;
	struct std::mutex_state *state = NULL;

	if (curr->is_trylock() || curr->is_lock() || curr->is_unlock()) {
		mutex = (std::mutex *)curr->get_location();
		state = mutex->get_state();
	} else if (curr->is_wait()) {
		mutex = (std::mutex *)curr->get_value();
		state = mutex->get_state();
	}

	switch (curr->get_type()) {
	case ATOMIC_TRYLOCK: {
		bool success = !state->islocked;
		curr->set_try_lock(success);
		if (!success) {
			get_thread(curr)->set_return_value(0);
			break;
		}
		get_thread(curr)->set_return_value(1);
	}
		//otherwise fall into the lock case
	case ATOMIC_LOCK: {
		if (curr->get_cv()->getClock(state->alloc_tid) <= state->alloc_clock)
			assert_bug("Lock access before initialization");
		state->islocked = true;
		ModelAction *unlock = get_last_unlock(curr);
		//synchronize with the previous unlock statement
		if (unlock != NULL) {
			curr->synchronize_with(unlock);
			return true;
		}
		break;
	}
	case ATOMIC_UNLOCK: {
		//unlock the lock
		state->islocked = false;
		//wake up the other threads
		action_list_t *waiters = get_safe_ptr_action(lock_waiters_map, curr->get_location());
		//activate all the waiting threads
		for (action_list_t::iterator rit = waiters->begin(); rit != waiters->end(); rit++) {
			scheduler->wake(get_thread(*rit));
		}
		waiters->clear();
		break;
	}
	case ATOMIC_WAIT: {
		//unlock the lock
		state->islocked = false;
		//wake up the other threads
		action_list_t *waiters = get_safe_ptr_action(lock_waiters_map, (void *) curr->get_value());
		//activate all the waiting threads
		for (action_list_t::iterator rit = waiters->begin(); rit != waiters->end(); rit++) {
			scheduler->wake(get_thread(*rit));
		}
		waiters->clear();
		//check whether we should go to sleep or not...simulate spurious failures
		if (curr->get_node()->get_misc() == 0) {
			get_safe_ptr_action(condvar_waiters_map, curr->get_location())->push_back(curr);
			//disable us
			scheduler->sleep(get_thread(curr));
		}
		break;
	}
	case ATOMIC_NOTIFY_ALL: {
		action_list_t *waiters = get_safe_ptr_action(condvar_waiters_map, curr->get_location());
		//activate all the waiting threads
		for (action_list_t::iterator rit = waiters->begin(); rit != waiters->end(); rit++) {
			scheduler->wake(get_thread(*rit));
		}
		waiters->clear();
		break;
	}
	case ATOMIC_NOTIFY_ONE: {
		action_list_t *waiters = get_safe_ptr_action(condvar_waiters_map, curr->get_location());
		int wakeupthread = curr->get_node()->get_misc();
		action_list_t::iterator it = waiters->begin();
		advance(it, wakeupthread);
		scheduler->wake(get_thread(*it));
		waiters->erase(it);
		break;
	}

	default:
		ASSERT(0);
	}
	return false;
}

void ModelChecker::add_future_value(const ModelAction *writer, ModelAction *reader)
{
	/* Do more ambitious checks now that mo is more complete */
	if (mo_may_allow(writer, reader) &&
			reader->get_node()->add_future_value(writer->get_value(),
				writer->get_seq_number() + params.maxfuturedelay))
		set_latest_backtrack(reader);
}

/**
 * Process a write ModelAction
 * @param curr The ModelAction to process
 * @return True if the mo_graph was updated or promises were resolved
 */
bool ModelChecker::process_write(ModelAction *curr)
{
	bool updated_mod_order = w_modification_order(curr);
	bool updated_promises = resolve_promises(curr);

	if (promises->size() == 0) {
		for (unsigned int i = 0; i < futurevalues->size(); i++) {
			struct PendingFutureValue pfv = (*futurevalues)[i];
			add_future_value(pfv.writer, pfv.act);
		}
		futurevalues->clear();
	}

	mo_graph->commitChanges();
	mo_check_promises(curr->get_tid(), curr);

	get_thread(curr)->set_return_value(VALUE_NONE);
	return updated_mod_order || updated_promises;
}

/**
 * Process a fence ModelAction
 * @param curr The ModelAction to process
 * @return True if synchronization was updated
 */
bool ModelChecker::process_fence(ModelAction *curr)
{
	/*
	 * fence-relaxed: no-op
	 * fence-release: only log the occurence (not in this function), for
	 *   use in later synchronization
	 * fence-acquire (this function): search for hypothetical release
	 *   sequences
	 */
	bool updated = false;
	if (curr->is_acquire()) {
		action_list_t *list = action_trace;
		action_list_t::reverse_iterator rit;
		/* Find X : is_read(X) && X --sb-> curr */
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;
			if (act == curr)
				continue;
			if (act->get_tid() != curr->get_tid())
				continue;
			/* Stop at the beginning of the thread */
			if (act->is_thread_start())
				break;
			/* Stop once we reach a prior fence-acquire */
			if (act->is_fence() && act->is_acquire())
				break;
			if (!act->is_read())
				continue;
			/* read-acquire will find its own release sequences */
			if (act->is_acquire())
				continue;

			/* Establish hypothetical release sequences */
			rel_heads_list_t release_heads;
			get_release_seq_heads(curr, act, &release_heads);
			for (unsigned int i = 0; i < release_heads.size(); i++)
				if (!curr->synchronize_with(release_heads[i]))
					set_bad_synchronization();
			if (release_heads.size() != 0)
				updated = true;
		}
	}
	return updated;
}

/**
 * @brief Process the current action for thread-related activity
 *
 * Performs current-action processing for a THREAD_* ModelAction. Proccesses
 * may include setting Thread status, completing THREAD_FINISH/THREAD_JOIN
 * synchronization, etc.  This function is a no-op for non-THREAD actions
 * (e.g., ATOMIC_{READ,WRITE,RMW,LOCK}, etc.)
 *
 * @param curr The current action
 * @return True if synchronization was updated or a thread completed
 */
bool ModelChecker::process_thread_action(ModelAction *curr)
{
	bool updated = false;

	switch (curr->get_type()) {
	case THREAD_CREATE: {
		Thread *th = curr->get_thread_operand();
		th->set_creation(curr);
		break;
	}
	case THREAD_JOIN: {
		Thread *blocking = curr->get_thread_operand();
		ModelAction *act = get_last_action(blocking->get_id());
		curr->synchronize_with(act);
		updated = true; /* trigger rel-seq checks */
		break;
	}
	case THREAD_FINISH: {
		Thread *th = get_thread(curr);
		while (!th->wait_list_empty()) {
			ModelAction *act = th->pop_wait_list();
			scheduler->wake(get_thread(act));
		}
		th->complete();
		updated = true; /* trigger rel-seq checks */
		break;
	}
	case THREAD_START: {
		check_promises(curr->get_tid(), NULL, curr->get_cv());
		break;
	}
	default:
		break;
	}

	return updated;
}

/**
 * @brief Process the current action for release sequence fixup activity
 *
 * Performs model-checker release sequence fixups for the current action,
 * forcing a single pending release sequence to break (with a given, potential
 * "loose" write) or to complete (i.e., synchronize). If a pending release
 * sequence forms a complete release sequence, then we must perform the fixup
 * synchronization, mo_graph additions, etc.
 *
 * @param curr The current action; must be a release sequence fixup action
 * @param work_queue The work queue to which to add work items as they are
 * generated
 */
void ModelChecker::process_relseq_fixup(ModelAction *curr, work_queue_t *work_queue)
{
	const ModelAction *write = curr->get_node()->get_relseq_break();
	struct release_seq *sequence = pending_rel_seqs->back();
	pending_rel_seqs->pop_back();
	ASSERT(sequence);
	ModelAction *acquire = sequence->acquire;
	const ModelAction *rf = sequence->rf;
	const ModelAction *release = sequence->release;
	ASSERT(acquire);
	ASSERT(release);
	ASSERT(rf);
	ASSERT(release->same_thread(rf));

	if (write == NULL) {
		/**
		 * @todo Forcing a synchronization requires that we set
		 * modification order constraints. For instance, we can't allow
		 * a fixup sequence in which two separate read-acquire
		 * operations read from the same sequence, where the first one
		 * synchronizes and the other doesn't. Essentially, we can't
		 * allow any writes to insert themselves between 'release' and
		 * 'rf'
		 */

		/* Must synchronize */
		if (!acquire->synchronize_with(release)) {
			set_bad_synchronization();
			return;
		}
		/* Re-check all pending release sequences */
		work_queue->push_back(CheckRelSeqWorkEntry(NULL));
		/* Re-check act for mo_graph edges */
		work_queue->push_back(MOEdgeWorkEntry(acquire));

		/* propagate synchronization to later actions */
		action_list_t::reverse_iterator rit = action_trace->rbegin();
		for (; (*rit) != acquire; rit++) {
			ModelAction *propagate = *rit;
			if (acquire->happens_before(propagate)) {
				propagate->synchronize_with(acquire);
				/* Re-check 'propagate' for mo_graph edges */
				work_queue->push_back(MOEdgeWorkEntry(propagate));
			}
		}
	} else {
		/* Break release sequence with new edges:
		 *   release --mo--> write --mo--> rf */
		mo_graph->addEdge(release, write);
		mo_graph->addEdge(write, rf);
	}

	/* See if we have realized a data race */
	checkDataRaces();
}

/**
 * Initialize the current action by performing one or more of the following
 * actions, as appropriate: merging RMWR and RMWC/RMW actions, stepping forward
 * in the NodeStack, manipulating backtracking sets, allocating and
 * initializing clock vectors, and computing the promises to fulfill.
 *
 * @param curr The current action, as passed from the user context; may be
 * freed/invalidated after the execution of this function, with a different
 * action "returned" its place (pass-by-reference)
 * @return True if curr is a newly-explored action; false otherwise
 */
bool ModelChecker::initialize_curr_action(ModelAction **curr)
{
	ModelAction *newcurr;

	if ((*curr)->is_rmwc() || (*curr)->is_rmw()) {
		newcurr = process_rmw(*curr);
		delete *curr;

		if (newcurr->is_rmw())
			compute_promises(newcurr);

		*curr = newcurr;
		return false;
	}

	(*curr)->set_seq_number(get_next_seq_num());

	newcurr = node_stack->explore_action(*curr, scheduler->get_enabled_array());
	if (newcurr) {
		/* First restore type and order in case of RMW operation */
		if ((*curr)->is_rmwr())
			newcurr->copy_typeandorder(*curr);

		ASSERT((*curr)->get_location() == newcurr->get_location());
		newcurr->copy_from_new(*curr);

		/* Discard duplicate ModelAction; use action from NodeStack */
		delete *curr;

		/* Always compute new clock vector */
		newcurr->create_cv(get_parent_action(newcurr->get_tid()));

		*curr = newcurr;
		return false; /* Action was explored previously */
	} else {
		newcurr = *curr;

		/* Always compute new clock vector */
		newcurr->create_cv(get_parent_action(newcurr->get_tid()));

		/* Assign most recent release fence */
		newcurr->set_last_fence_release(get_last_fence_release(newcurr->get_tid()));

		/*
		 * Perform one-time actions when pushing new ModelAction onto
		 * NodeStack
		 */
		if (newcurr->is_write())
			compute_promises(newcurr);
		else if (newcurr->is_relseq_fixup())
			compute_relseq_breakwrites(newcurr);
		else if (newcurr->is_wait())
			newcurr->get_node()->set_misc_max(2);
		else if (newcurr->is_notify_one()) {
			newcurr->get_node()->set_misc_max(get_safe_ptr_action(condvar_waiters_map, newcurr->get_location())->size());
		}
		return true; /* This was a new ModelAction */
	}
}

/**
 * @brief Establish reads-from relation between two actions
 *
 * Perform basic operations involved with establishing a concrete rf relation,
 * including setting the ModelAction data and checking for release sequences.
 *
 * @param act The action that is reading (must be a read)
 * @param rf The action from which we are reading (must be a write)
 *
 * @return True if this read established synchronization
 */
bool ModelChecker::read_from(ModelAction *act, const ModelAction *rf)
{
	act->set_read_from(rf);
	if (rf != NULL && act->is_acquire()) {
		rel_heads_list_t release_heads;
		get_release_seq_heads(act, act, &release_heads);
		int num_heads = release_heads.size();
		for (unsigned int i = 0; i < release_heads.size(); i++)
			if (!act->synchronize_with(release_heads[i])) {
				set_bad_synchronization();
				num_heads--;
			}
		return num_heads > 0;
	}
	return false;
}

/**
 * @brief Check whether a model action is enabled.
 *
 * Checks whether a lock or join operation would be successful (i.e., is the
 * lock already locked, or is the joined thread already complete). If not, put
 * the action in a waiter list.
 *
 * @param curr is the ModelAction to check whether it is enabled.
 * @return a bool that indicates whether the action is enabled.
 */
bool ModelChecker::check_action_enabled(ModelAction *curr) {
	if (curr->is_lock()) {
		std::mutex *lock = (std::mutex *)curr->get_location();
		struct std::mutex_state *state = lock->get_state();
		if (state->islocked) {
			//Stick the action in the appropriate waiting queue
			get_safe_ptr_action(lock_waiters_map, curr->get_location())->push_back(curr);
			return false;
		}
	} else if (curr->get_type() == THREAD_JOIN) {
		Thread *blocking = (Thread *)curr->get_location();
		if (!blocking->is_complete()) {
			blocking->push_wait_list(curr);
			return false;
		}
	}

	return true;
}

/**
 * Stores the ModelAction for the current thread action.  Call this
 * immediately before switching from user- to system-context to pass
 * data between them.
 * @param act The ModelAction created by the user-thread action
 */
void ModelChecker::set_current_action(ModelAction *act) {
	priv->current_action = act;
}

/**
 * This is the heart of the model checker routine. It performs model-checking
 * actions corresponding to a given "current action." Among other processes, it
 * calculates reads-from relationships, updates synchronization clock vectors,
 * forms a memory_order constraints graph, and handles replay/backtrack
 * execution when running permutations of previously-observed executions.
 *
 * @param curr The current action to process
 * @return The ModelAction that is actually executed; may be different than
 * curr; may be NULL, if the current action is not enabled to run
 */
ModelAction * ModelChecker::check_current_action(ModelAction *curr)
{
	ASSERT(curr);
	bool second_part_of_rmw = curr->is_rmwc() || curr->is_rmw();

	if (!check_action_enabled(curr)) {
		/* Make the execution look like we chose to run this action
		 * much later, when a lock/join can succeed */
		get_thread(curr)->set_pending(curr);
		scheduler->sleep(get_thread(curr));
		return NULL;
	}

	bool newly_explored = initialize_curr_action(&curr);

	DBG();
	if (DBG_ENABLED())
		curr->print();

	wake_up_sleeping_actions(curr);

	/* Add the action to lists before any other model-checking tasks */
	if (!second_part_of_rmw)
		add_action_to_lists(curr);

	/* Build may_read_from set for newly-created actions */
	if (newly_explored && curr->is_read())
		build_reads_from_past(curr);

	/* Initialize work_queue with the "current action" work */
	work_queue_t work_queue(1, CheckCurrWorkEntry(curr));
	while (!work_queue.empty() && !has_asserted()) {
		WorkQueueEntry work = work_queue.front();
		work_queue.pop_front();

		switch (work.type) {
		case WORK_CHECK_CURR_ACTION: {
			ModelAction *act = work.action;
			bool update = false; /* update this location's release seq's */
			bool update_all = false; /* update all release seq's */

			if (process_thread_action(curr))
				update_all = true;

			if (act->is_read() && process_read(act, second_part_of_rmw))
				update = true;

			if (act->is_write() && process_write(act))
				update = true;

			if (act->is_fence() && process_fence(act))
				update_all = true;

			if (act->is_mutex_op() && process_mutex(act))
				update_all = true;

			if (act->is_relseq_fixup())
				process_relseq_fixup(curr, &work_queue);

			if (update_all)
				work_queue.push_back(CheckRelSeqWorkEntry(NULL));
			else if (update)
				work_queue.push_back(CheckRelSeqWorkEntry(act->get_location()));
			break;
		}
		case WORK_CHECK_RELEASE_SEQ:
			resolve_release_sequences(work.location, &work_queue);
			break;
		case WORK_CHECK_MO_EDGES: {
			/** @todo Complete verification of work_queue */
			ModelAction *act = work.action;
			bool updated = false;

			if (act->is_read()) {
				const ModelAction *rf = act->get_reads_from();
				if (rf != NULL && r_modification_order(act, rf))
					updated = true;
			}
			if (act->is_write()) {
				if (w_modification_order(act))
					updated = true;
			}
			mo_graph->commitChanges();

			if (updated)
				work_queue.push_back(CheckRelSeqWorkEntry(act->get_location()));
			break;
		}
		default:
			ASSERT(false);
			break;
		}
	}

	check_curr_backtracking(curr);
	set_backtracking(curr);
	return curr;
}

void ModelChecker::check_curr_backtracking(ModelAction *curr)
{
	Node *currnode = curr->get_node();
	Node *parnode = currnode->get_parent();

	if ((parnode && !parnode->backtrack_empty()) ||
			 !currnode->misc_empty() ||
			 !currnode->read_from_empty() ||
			 !currnode->future_value_empty() ||
			 !currnode->promise_empty() ||
			 !currnode->relseq_break_empty()) {
		set_latest_backtrack(curr);
	}
}

bool ModelChecker::promises_expired() const
{
	for (unsigned int i = 0; i < promises->size(); i++) {
		Promise *promise = (*promises)[i];
		if (promise->get_expiration() < priv->used_sequence_numbers)
			return true;
	}
	return false;
}

/**
 * This is the strongest feasibility check available.
 * @return whether the current trace (partial or complete) must be a prefix of
 * a feasible trace.
 */
bool ModelChecker::isfeasibleprefix() const
{
	return pending_rel_seqs->size() == 0 && is_feasible_prefix_ignore_relseq();
}

/**
 * Print disagnostic information about an infeasible execution
 * @param prefix A string to prefix the output with; if NULL, then a default
 * message prefix will be provided
 */
void ModelChecker::print_infeasibility(const char *prefix) const
{
	char buf[100];
	char *ptr = buf;
	if (mo_graph->checkForRMWViolation())
		ptr += sprintf(ptr, "[RMW atomicity]");
	if (mo_graph->checkForCycles())
		ptr += sprintf(ptr, "[mo cycle]");
	if (priv->failed_promise)
		ptr += sprintf(ptr, "[failed promise]");
	if (priv->too_many_reads)
		ptr += sprintf(ptr, "[too many reads]");
	if (priv->bad_synchronization)
		ptr += sprintf(ptr, "[bad sw ordering]");
	if (promises_expired())
		ptr += sprintf(ptr, "[promise expired]");
	if (promises->size() != 0)
		ptr += sprintf(ptr, "[unrevolved promise]");
	if (ptr != buf)
		model_print("%s: %s\n", prefix ? prefix : "Infeasible", buf);
}

/**
 * Returns whether the current completed trace is feasible, except for pending
 * release sequences.
 */
bool ModelChecker::is_feasible_prefix_ignore_relseq() const
{
	return !is_infeasible() && promises->size() == 0;
}

/**
 * Check if the current partial trace is infeasible. Does not check any
 * end-of-execution flags, which might rule out the execution. Thus, this is
 * useful only for ruling an execution as infeasible.
 * @return whether the current partial trace is infeasible.
 */
bool ModelChecker::is_infeasible() const
{
	return mo_graph->checkForRMWViolation() || is_infeasible_ignoreRMW();
}

/**
 * Check If the current partial trace is infeasible, while ignoring
 * infeasibility related to 2 RMW's reading from the same store. It does not
 * check end-of-execution feasibility.
 * @see ModelChecker::is_infeasible
 * @return whether the current partial trace is infeasible, ignoring multiple
 * RMWs reading from the same store.
 * */
bool ModelChecker::is_infeasible_ignoreRMW() const
{
	return mo_graph->checkForCycles() || priv->failed_promise ||
		priv->too_many_reads || priv->bad_synchronization ||
		promises_expired();
}

/** Close out a RMWR by converting previous RMWR into a RMW or READ. */
ModelAction * ModelChecker::process_rmw(ModelAction *act) {
	ModelAction *lastread = get_last_action(act->get_tid());
	lastread->process_rmw(act);
	if (act->is_rmw() && lastread->get_reads_from() != NULL) {
		mo_graph->addRMWEdge(lastread->get_reads_from(), lastread);
		mo_graph->commitChanges();
	}
	return lastread;
}

/**
 * Checks whether a thread has read from the same write for too many times
 * without seeing the effects of a later write.
 *
 * Basic idea:
 * 1) there must a different write that we could read from that would satisfy the modification order,
 * 2) we must have read from the same value in excess of maxreads times, and
 * 3) that other write must have been in the reads_from set for maxreads times.
 *
 * If so, we decide that the execution is no longer feasible.
 */
void ModelChecker::check_recency(ModelAction *curr, const ModelAction *rf)
{
	if (params.maxreads != 0) {
		if (curr->get_node()->get_read_from_size() <= 1)
			return;
		//Must make sure that execution is currently feasible...  We could
		//accidentally clear by rolling back
		if (is_infeasible())
			return;
		std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, curr->get_location());
		int tid = id_to_int(curr->get_tid());

		/* Skip checks */
		if ((int)thrd_lists->size() <= tid)
			return;
		action_list_t *list = &(*thrd_lists)[tid];

		action_list_t::reverse_iterator rit = list->rbegin();
		/* Skip past curr */
		for (; (*rit) != curr; rit++)
			;
		/* go past curr now */
		rit++;

		action_list_t::reverse_iterator ritcopy = rit;
		//See if we have enough reads from the same value
		int count = 0;
		for (; count < params.maxreads; rit++, count++) {
			if (rit == list->rend())
				return;
			ModelAction *act = *rit;
			if (!act->is_read())
				return;

			if (act->get_reads_from() != rf)
				return;
			if (act->get_node()->get_read_from_size() <= 1)
				return;
		}
		for (int i = 0; i < curr->get_node()->get_read_from_size(); i++) {
			/* Get write */
			const ModelAction *write = curr->get_node()->get_read_from_at(i);

			/* Need a different write */
			if (write == rf)
				continue;

			/* Test to see whether this is a feasible write to read from */
			mo_graph->startChanges();
			r_modification_order(curr, write);
			bool feasiblereadfrom = !is_infeasible();
			mo_graph->rollbackChanges();

			if (!feasiblereadfrom)
				continue;
			rit = ritcopy;

			bool feasiblewrite = true;
			//new we need to see if this write works for everyone

			for (int loop = count; loop > 0; loop--, rit++) {
				ModelAction *act = *rit;
				bool foundvalue = false;
				for (int j = 0; j < act->get_node()->get_read_from_size(); j++) {
					if (act->get_node()->get_read_from_at(j) == write) {
						foundvalue = true;
						break;
					}
				}
				if (!foundvalue) {
					feasiblewrite = false;
					break;
				}
			}
			if (feasiblewrite) {
				priv->too_many_reads = true;
				return;
			}
		}
	}
}

/**
 * Updates the mo_graph with the constraints imposed from the current
 * read.
 *
 * Basic idea is the following: Go through each other thread and find
 * the lastest action that happened before our read.  Two cases:
 *
 * (1) The action is a write => that write must either occur before
 * the write we read from or be the write we read from.
 *
 * (2) The action is a read => the write that that action read from
 * must occur before the write we read from or be the same write.
 *
 * @param curr The current action. Must be a read.
 * @param rf The action that curr reads from. Must be a write.
 * @return True if modification order edges were added; false otherwise
 */
bool ModelChecker::r_modification_order(ModelAction *curr, const ModelAction *rf)
{
	std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, curr->get_location());
	unsigned int i;
	bool added = false;
	ASSERT(curr->is_read());

	/* Last SC fence in the current thread */
	ModelAction *last_sc_fence_local = get_last_seq_cst_fence(curr->get_tid(), NULL);

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Last SC fence in thread i */
		ModelAction *last_sc_fence_thread_local = NULL;
		if (int_to_id((int)i) != curr->get_tid())
			last_sc_fence_thread_local = get_last_seq_cst_fence(int_to_id(i), NULL);

		/* Last SC fence in thread i, before last SC fence in current thread */
		ModelAction *last_sc_fence_thread_before = NULL;
		if (last_sc_fence_local)
			last_sc_fence_thread_before = get_last_seq_cst_fence(int_to_id(i), last_sc_fence_local);

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			if (act->is_write() && act != rf && act != curr) {
				/* C++, Section 29.3 statement 5 */
				if (curr->is_seqcst() && last_sc_fence_thread_local &&
						*act < *last_sc_fence_thread_local) {
					mo_graph->addEdge(act, rf);
					added = true;
					break;
				}
				/* C++, Section 29.3 statement 4 */
				else if (act->is_seqcst() && last_sc_fence_local &&
						*act < *last_sc_fence_local) {
					mo_graph->addEdge(act, rf);
					added = true;
					break;
				}
				/* C++, Section 29.3 statement 6 */
				else if (last_sc_fence_thread_before &&
						*act < *last_sc_fence_thread_before) {
					mo_graph->addEdge(act, rf);
					added = true;
					break;
				}
			}

			/*
			 * Include at most one act per-thread that "happens
			 * before" curr. Don't consider reflexively.
			 */
			if (act->happens_before(curr) && act != curr) {
				if (act->is_write()) {
					if (rf != act) {
						mo_graph->addEdge(act, rf);
						added = true;
					}
				} else {
					const ModelAction *prevreadfrom = act->get_reads_from();
					//if the previous read is unresolved, keep going...
					if (prevreadfrom == NULL)
						continue;

					if (rf != prevreadfrom) {
						mo_graph->addEdge(prevreadfrom, rf);
						added = true;
					}
				}
				break;
			}
		}
	}

	return added;
}

/** This method fixes up the modification order when we resolve a
 *  promises.  The basic problem is that actions that occur after the
 *  read curr could not property add items to the modification order
 *  for our read.
 *
 *  So for each thread, we find the earliest item that happens after
 *  the read curr.  This is the item we have to fix up with additional
 *  constraints.  If that action is write, we add a MO edge between
 *  the Action rf and that action.  If the action is a read, we add a
 *  MO edge between the Action rf, and whatever the read accessed.
 *
 * @param curr is the read ModelAction that we are fixing up MO edges for.
 * @param rf is the write ModelAction that curr reads from.
 *
 */
void ModelChecker::post_r_modification_order(ModelAction *curr, const ModelAction *rf)
{
	std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		ModelAction *lastact = NULL;

		/* Find last action that happens after curr that is either not curr or a rmw */
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;
			if (curr->happens_before(act) && (curr != act || curr->is_rmw())) {
				lastact = act;
			} else
				break;
		}

			/* Include at most one act per-thread that "happens before" curr */
		if (lastact != NULL) {
			if (lastact == curr) {
				//Case 1: The resolved read is a RMW, and we need to make sure
				//that the write portion of the RMW mod order after rf

				mo_graph->addEdge(rf, lastact);
			} else if (lastact->is_read()) {
				//Case 2: The resolved read is a normal read and the next
				//operation is a read, and we need to make sure the value read
				//is mod ordered after rf

				const ModelAction *postreadfrom = lastact->get_reads_from();
				if (postreadfrom != NULL && rf != postreadfrom)
					mo_graph->addEdge(rf, postreadfrom);
			} else {
				//Case 3: The resolved read is a normal read and the next
				//operation is a write, and we need to make sure that the
				//write is mod ordered after rf
				if (lastact != rf)
					mo_graph->addEdge(rf, lastact);
			}
			break;
		}
	}
}

/**
 * Updates the mo_graph with the constraints imposed from the current write.
 *
 * Basic idea is the following: Go through each other thread and find
 * the lastest action that happened before our write.  Two cases:
 *
 * (1) The action is a write => that write must occur before
 * the current write
 *
 * (2) The action is a read => the write that that action read from
 * must occur before the current write.
 *
 * This method also handles two other issues:
 *
 * (I) Sequential Consistency: Making sure that if the current write is
 * seq_cst, that it occurs after the previous seq_cst write.
 *
 * (II) Sending the write back to non-synchronizing reads.
 *
 * @param curr The current action. Must be a write.
 * @return True if modification order edges were added; false otherwise
 */
bool ModelChecker::w_modification_order(ModelAction *curr)
{
	std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, curr->get_location());
	unsigned int i;
	bool added = false;
	ASSERT(curr->is_write());

	if (curr->is_seqcst()) {
		/* We have to at least see the last sequentially consistent write,
			 so we are initialized. */
		ModelAction *last_seq_cst = get_last_seq_cst_write(curr);
		if (last_seq_cst != NULL) {
			mo_graph->addEdge(last_seq_cst, curr);
			added = true;
		}
	}

	/* Last SC fence in the current thread */
	ModelAction *last_sc_fence_local = get_last_seq_cst_fence(curr->get_tid(), NULL);

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Last SC fence in thread i, before last SC fence in current thread */
		ModelAction *last_sc_fence_thread_before = NULL;
		if (last_sc_fence_local && int_to_id((int)i) != curr->get_tid())
			last_sc_fence_thread_before = get_last_seq_cst_fence(int_to_id(i), last_sc_fence_local);

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;
			if (act == curr) {
				/*
				 * 1) If RMW and it actually read from something, then we
				 * already have all relevant edges, so just skip to next
				 * thread.
				 *
				 * 2) If RMW and it didn't read from anything, we should
				 * whatever edge we can get to speed up convergence.
				 *
				 * 3) If normal write, we need to look at earlier actions, so
				 * continue processing list.
				 */
				if (curr->is_rmw()) {
					if (curr->get_reads_from() != NULL)
						break;
					else
						continue;
				} else
					continue;
			}

			/* C++, Section 29.3 statement 7 */
			if (last_sc_fence_thread_before && act->is_write() &&
					*act < *last_sc_fence_thread_before) {
				mo_graph->addEdge(act, curr);
				added = true;
				break;
			}

			/*
			 * Include at most one act per-thread that "happens
			 * before" curr
			 */
			if (act->happens_before(curr)) {
				/*
				 * Note: if act is RMW, just add edge:
				 *   act --mo--> curr
				 * The following edge should be handled elsewhere:
				 *   readfrom(act) --mo--> act
				 */
				if (act->is_write())
					mo_graph->addEdge(act, curr);
				else if (act->is_read()) {
					//if previous read accessed a null, just keep going
					if (act->get_reads_from() == NULL)
						continue;
					mo_graph->addEdge(act->get_reads_from(), curr);
				}
				added = true;
				break;
			} else if (act->is_read() && !act->could_synchronize_with(curr) &&
			                             !act->same_thread(curr)) {
				/* We have an action that:
				   (1) did not happen before us
				   (2) is a read and we are a write
				   (3) cannot synchronize with us
				   (4) is in a different thread
				   =>
				   that read could potentially read from our write.  Note that
				   these checks are overly conservative at this point, we'll
				   do more checks before actually removing the
				   pendingfuturevalue.

				 */
				if (thin_air_constraint_may_allow(curr, act)) {
					if (!is_infeasible() ||
							(curr->is_rmw() && act->is_rmw() && curr->get_reads_from() == act->get_reads_from() && !is_infeasible_ignoreRMW())) {
						futurevalues->push_back(PendingFutureValue(curr, act));
					}
				}
			}
		}
	}

	return added;
}

/** Arbitrary reads from the future are not allowed.  Section 29.3
 * part 9 places some constraints.  This method checks one result of constraint
 * constraint.  Others require compiler support. */
bool ModelChecker::thin_air_constraint_may_allow(const ModelAction *writer, const ModelAction *reader)
{
	if (!writer->is_rmw())
		return true;

	if (!reader->is_rmw())
		return true;

	for (const ModelAction *search = writer->get_reads_from(); search != NULL; search = search->get_reads_from()) {
		if (search == reader)
			return false;
		if (search->get_tid() == reader->get_tid() &&
				search->happens_before(reader))
			break;
	}

	return true;
}

/**
 * Arbitrary reads from the future are not allowed. Section 29.3 part 9 places
 * some constraints. This method checks one the following constraint (others
 * require compiler support):
 *
 *   If X --hb-> Y --mo-> Z, then X should not read from Z.
 */
bool ModelChecker::mo_may_allow(const ModelAction *writer, const ModelAction *reader)
{
	std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, reader->get_location());
	unsigned int i;
	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		const ModelAction *write_after_read = NULL;

		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Don't disallow due to act == reader */
			if (!reader->happens_before(act) || reader == act)
				break;
			else if (act->is_write())
				write_after_read = act;
			else if (act->is_read() && act->get_reads_from() != NULL)
				write_after_read = act->get_reads_from();
		}

		if (write_after_read && write_after_read != writer && mo_graph->checkReachable(write_after_read, writer))
			return false;
	}
	return true;
}

/**
 * Finds the head(s) of the release sequence(s) containing a given ModelAction.
 * The ModelAction under consideration is expected to be taking part in
 * release/acquire synchronization as an object of the "reads from" relation.
 * Note that this can only provide release sequence support for RMW chains
 * which do not read from the future, as those actions cannot be traced until
 * their "promise" is fulfilled. Similarly, we may not even establish the
 * presence of a release sequence with certainty, as some modification order
 * constraints may be decided further in the future. Thus, this function
 * "returns" two pieces of data: a pass-by-reference vector of @a release_heads
 * and a boolean representing certainty.
 *
 * @param rf The action that might be part of a release sequence. Must be a
 * write.
 * @param release_heads A pass-by-reference style return parameter. After
 * execution of this function, release_heads will contain the heads of all the
 * relevant release sequences, if any exists with certainty
 * @param pending A pass-by-reference style return parameter which is only used
 * when returning false (i.e., uncertain). Returns most information regarding
 * an uncertain release sequence, including any write operations that might
 * break the sequence.
 * @return true, if the ModelChecker is certain that release_heads is complete;
 * false otherwise
 */
bool ModelChecker::release_seq_heads(const ModelAction *rf,
		rel_heads_list_t *release_heads,
		struct release_seq *pending) const
{
	/* Only check for release sequences if there are no cycles */
	if (mo_graph->checkForCycles())
		return false;

	while (rf) {
		ASSERT(rf->is_write());

		if (rf->is_release())
			release_heads->push_back(rf);
		else if (rf->get_last_fence_release())
			release_heads->push_back(rf->get_last_fence_release());
		if (!rf->is_rmw())
			break; /* End of RMW chain */

		/** @todo Need to be smarter here...  In the linux lock
		 * example, this will run to the beginning of the program for
		 * every acquire. */
		/** @todo The way to be smarter here is to keep going until 1
		 * thread has a release preceded by an acquire and you've seen
		 *	 both. */

		/* acq_rel RMW is a sufficient stopping condition */
		if (rf->is_acquire() && rf->is_release())
			return true; /* complete */

		rf = rf->get_reads_from();
	};
	if (!rf) {
		/* read from future: need to settle this later */
		pending->rf = NULL;
		return false; /* incomplete */
	}

	if (rf->is_release())
		return true; /* complete */

	/* else relaxed write
	 * - check for fence-release in the same thread (29.8, stmt. 3)
	 * - check modification order for contiguous subsequence
	 *   -> rf must be same thread as release */

	const ModelAction *fence_release = rf->get_last_fence_release();
	/* Synchronize with a fence-release unconditionally; we don't need to
	 * find any more "contiguous subsequence..." for it */
	if (fence_release)
		release_heads->push_back(fence_release);

	int tid = id_to_int(rf->get_tid());
	std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, rf->get_location());
	action_list_t *list = &(*thrd_lists)[tid];
	action_list_t::const_reverse_iterator rit;

	/* Find rf in the thread list */
	rit = std::find(list->rbegin(), list->rend(), rf);
	ASSERT(rit != list->rend());

	/* Find the last {write,fence}-release */
	for (; rit != list->rend(); rit++) {
		if (fence_release && *(*rit) < *fence_release)
			break;
		if ((*rit)->is_release())
			break;
	}
	if (rit == list->rend()) {
		/* No write-release in this thread */
		return true; /* complete */
	} else if (fence_release && *(*rit) < *fence_release) {
		/* The fence-release is more recent (and so, "stronger") than
		 * the most recent write-release */
		return true; /* complete */
	} /* else, need to establish contiguous release sequence */
	ModelAction *release = *rit;

	ASSERT(rf->same_thread(release));

	pending->writes.clear();

	bool certain = true;
	for (unsigned int i = 0; i < thrd_lists->size(); i++) {
		if (id_to_int(rf->get_tid()) == (int)i)
			continue;
		list = &(*thrd_lists)[i];

		/* Can we ensure no future writes from this thread may break
		 * the release seq? */
		bool future_ordered = false;

		ModelAction *last = get_last_action(int_to_id(i));
		Thread *th = get_thread(int_to_id(i));
		if ((last && rf->happens_before(last)) ||
				!is_enabled(th) ||
				th->is_complete())
			future_ordered = true;

		ASSERT(!th->is_model_thread() || future_ordered);

		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			const ModelAction *act = *rit;
			/* Reach synchronization -> this thread is complete */
			if (act->happens_before(release))
				break;
			if (rf->happens_before(act)) {
				future_ordered = true;
				continue;
			}

			/* Only non-RMW writes can break release sequences */
			if (!act->is_write() || act->is_rmw())
				continue;

			/* Check modification order */
			if (mo_graph->checkReachable(rf, act)) {
				/* rf --mo--> act */
				future_ordered = true;
				continue;
			}
			if (mo_graph->checkReachable(act, release))
				/* act --mo--> release */
				break;
			if (mo_graph->checkReachable(release, act) &&
			              mo_graph->checkReachable(act, rf)) {
				/* release --mo-> act --mo--> rf */
				return true; /* complete */
			}
			/* act may break release sequence */
			pending->writes.push_back(act);
			certain = false;
		}
		if (!future_ordered)
			certain = false; /* This thread is uncertain */
	}

	if (certain) {
		release_heads->push_back(release);
		pending->writes.clear();
	} else {
		pending->release = release;
		pending->rf = rf;
	}
	return certain;
}

/**
 * An interface for getting the release sequence head(s) with which a
 * given ModelAction must synchronize. This function only returns a non-empty
 * result when it can locate a release sequence head with certainty. Otherwise,
 * it may mark the internal state of the ModelChecker so that it will handle
 * the release sequence at a later time, causing @a acquire to update its
 * synchronization at some later point in execution.
 *
 * @param acquire The 'acquire' action that may synchronize with a release
 * sequence
 * @param read The read action that may read from a release sequence; this may
 * be the same as acquire, or else an earlier action in the same thread (i.e.,
 * when 'acquire' is a fence-acquire)
 * @param release_heads A pass-by-reference return parameter. Will be filled
 * with the head(s) of the release sequence(s), if they exists with certainty.
 * @see ModelChecker::release_seq_heads
 */
void ModelChecker::get_release_seq_heads(ModelAction *acquire,
		ModelAction *read, rel_heads_list_t *release_heads)
{
	const ModelAction *rf = read->get_reads_from();
	struct release_seq *sequence = (struct release_seq *)snapshot_calloc(1, sizeof(struct release_seq));
	sequence->acquire = acquire;
	sequence->read = read;

	if (!release_seq_heads(rf, release_heads, sequence)) {
		/* add act to 'lazy checking' list */
		pending_rel_seqs->push_back(sequence);
	} else {
		snapshot_free(sequence);
	}
}

/**
 * Attempt to resolve all stashed operations that might synchronize with a
 * release sequence for a given location. This implements the "lazy" portion of
 * determining whether or not a release sequence was contiguous, since not all
 * modification order information is present at the time an action occurs.
 *
 * @param location The location/object that should be checked for release
 * sequence resolutions. A NULL value means to check all locations.
 * @param work_queue The work queue to which to add work items as they are
 * generated
 * @return True if any updates occurred (new synchronization, new mo_graph
 * edges)
 */
bool ModelChecker::resolve_release_sequences(void *location, work_queue_t *work_queue)
{
	bool updated = false;
	std::vector< struct release_seq *, SnapshotAlloc<struct release_seq *> >::iterator it = pending_rel_seqs->begin();
	while (it != pending_rel_seqs->end()) {
		struct release_seq *pending = *it;
		ModelAction *acquire = pending->acquire;
		const ModelAction *read = pending->read;

		/* Only resolve sequences on the given location, if provided */
		if (location && read->get_location() != location) {
			it++;
			continue;
		}

		const ModelAction *rf = read->get_reads_from();
		rel_heads_list_t release_heads;
		bool complete;
		complete = release_seq_heads(rf, &release_heads, pending);
		for (unsigned int i = 0; i < release_heads.size(); i++) {
			if (!acquire->has_synchronized_with(release_heads[i])) {
				if (acquire->synchronize_with(release_heads[i]))
					updated = true;
				else
					set_bad_synchronization();
			}
		}

		if (updated) {
			/* Re-check all pending release sequences */
			work_queue->push_back(CheckRelSeqWorkEntry(NULL));
			/* Re-check read-acquire for mo_graph edges */
			if (acquire->is_read())
				work_queue->push_back(MOEdgeWorkEntry(acquire));

			/* propagate synchronization to later actions */
			action_list_t::reverse_iterator rit = action_trace->rbegin();
			for (; (*rit) != acquire; rit++) {
				ModelAction *propagate = *rit;
				if (acquire->happens_before(propagate)) {
					propagate->synchronize_with(acquire);
					/* Re-check 'propagate' for mo_graph edges */
					work_queue->push_back(MOEdgeWorkEntry(propagate));
				}
			}
		}
		if (complete) {
			it = pending_rel_seqs->erase(it);
			snapshot_free(pending);
		} else {
			it++;
		}
	}

	// If we resolved promises or data races, see if we have realized a data race.
	checkDataRaces();

	return updated;
}

/**
 * Performs various bookkeeping operations for the current ModelAction. For
 * instance, adds action to the per-object, per-thread action vector and to the
 * action trace list of all thread actions.
 *
 * @param act is the ModelAction to add.
 */
void ModelChecker::add_action_to_lists(ModelAction *act)
{
	int tid = id_to_int(act->get_tid());
	ModelAction *uninit = NULL;
	int uninit_id = -1;
	action_list_t *list = get_safe_ptr_action(obj_map, act->get_location());
	if (list->empty() && act->is_atomic_var()) {
		uninit = new_uninitialized_action(act->get_location());
		uninit_id = id_to_int(uninit->get_tid());
		list->push_back(uninit);
	}
	list->push_back(act);

	action_trace->push_back(act);
	if (uninit)
		action_trace->push_front(uninit);

	std::vector<action_list_t> *vec = get_safe_ptr_vect_action(obj_thrd_map, act->get_location());
	if (tid >= (int)vec->size())
		vec->resize(priv->next_thread_id);
	(*vec)[tid].push_back(act);
	if (uninit)
		(*vec)[uninit_id].push_front(uninit);

	if ((int)thrd_last_action->size() <= tid)
		thrd_last_action->resize(get_num_threads());
	(*thrd_last_action)[tid] = act;
	if (uninit)
		(*thrd_last_action)[uninit_id] = uninit;

	if (act->is_fence() && act->is_release()) {
		if ((int)thrd_last_fence_release->size() <= tid)
			thrd_last_fence_release->resize(get_num_threads());
		(*thrd_last_fence_release)[tid] = act;
	}

	if (act->is_wait()) {
		void *mutex_loc = (void *) act->get_value();
		get_safe_ptr_action(obj_map, mutex_loc)->push_back(act);

		std::vector<action_list_t> *vec = get_safe_ptr_vect_action(obj_thrd_map, mutex_loc);
		if (tid >= (int)vec->size())
			vec->resize(priv->next_thread_id);
		(*vec)[tid].push_back(act);
	}
}

/**
 * @brief Get the last action performed by a particular Thread
 * @param tid The thread ID of the Thread in question
 * @return The last action in the thread
 */
ModelAction * ModelChecker::get_last_action(thread_id_t tid) const
{
	int threadid = id_to_int(tid);
	if (threadid < (int)thrd_last_action->size())
		return (*thrd_last_action)[id_to_int(tid)];
	else
		return NULL;
}

/**
 * @brief Get the last fence release performed by a particular Thread
 * @param tid The thread ID of the Thread in question
 * @return The last fence release in the thread, if one exists; NULL otherwise
 */
ModelAction * ModelChecker::get_last_fence_release(thread_id_t tid) const
{
	int threadid = id_to_int(tid);
	if (threadid < (int)thrd_last_fence_release->size())
		return (*thrd_last_fence_release)[id_to_int(tid)];
	else
		return NULL;
}

/**
 * Gets the last memory_order_seq_cst write (in the total global sequence)
 * performed on a particular object (i.e., memory location), not including the
 * current action.
 * @param curr The current ModelAction; also denotes the object location to
 * check
 * @return The last seq_cst write
 */
ModelAction * ModelChecker::get_last_seq_cst_write(ModelAction *curr) const
{
	void *location = curr->get_location();
	action_list_t *list = get_safe_ptr_action(obj_map, location);
	/* Find: max({i in dom(S) | seq_cst(t_i) && isWrite(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin(); rit != list->rend(); rit++)
		if ((*rit)->is_write() && (*rit)->is_seqcst() && (*rit) != curr)
			return *rit;
	return NULL;
}

/**
 * Gets the last memory_order_seq_cst fence (in the total global sequence)
 * performed in a particular thread, prior to a particular fence.
 * @param tid The ID of the thread to check
 * @param before_fence The fence from which to begin the search; if NULL, then
 * search for the most recent fence in the thread.
 * @return The last prior seq_cst fence in the thread, if exists; otherwise, NULL
 */
ModelAction * ModelChecker::get_last_seq_cst_fence(thread_id_t tid, const ModelAction *before_fence) const
{
	/* All fences should have NULL location */
	action_list_t *list = get_safe_ptr_action(obj_map, NULL);
	action_list_t::reverse_iterator rit = list->rbegin();

	if (before_fence) {
		for (; rit != list->rend(); rit++)
			if (*rit == before_fence)
				break;

		ASSERT(*rit == before_fence);
		rit++;
	}

	for (; rit != list->rend(); rit++)
		if ((*rit)->is_fence() && (tid == (*rit)->get_tid()) && (*rit)->is_seqcst())
			return *rit;
	return NULL;
}

/**
 * Gets the last unlock operation performed on a particular mutex (i.e., memory
 * location). This function identifies the mutex according to the current
 * action, which is presumed to perform on the same mutex.
 * @param curr The current ModelAction; also denotes the object location to
 * check
 * @return The last unlock operation
 */
ModelAction * ModelChecker::get_last_unlock(ModelAction *curr) const
{
	void *location = curr->get_location();
	action_list_t *list = get_safe_ptr_action(obj_map, location);
	/* Find: max({i in dom(S) | isUnlock(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin(); rit != list->rend(); rit++)
		if ((*rit)->is_unlock() || (*rit)->is_wait())
			return *rit;
	return NULL;
}

ModelAction * ModelChecker::get_parent_action(thread_id_t tid) const
{
	ModelAction *parent = get_last_action(tid);
	if (!parent)
		parent = get_thread(tid)->get_creation();
	return parent;
}

/**
 * Returns the clock vector for a given thread.
 * @param tid The thread whose clock vector we want
 * @return Desired clock vector
 */
ClockVector * ModelChecker::get_cv(thread_id_t tid) const
{
	return get_parent_action(tid)->get_cv();
}

/**
 * Resolve a set of Promises with a current write. The set is provided in the
 * Node corresponding to @a write.
 * @param write The ModelAction that is fulfilling Promises
 * @return True if promises were resolved; false otherwise
 */
bool ModelChecker::resolve_promises(ModelAction *write)
{
	bool resolved = false;
	std::vector< thread_id_t, ModelAlloc<thread_id_t> > threads_to_check;

	for (unsigned int i = 0, promise_index = 0; promise_index < promises->size(); i++) {
		Promise *promise = (*promises)[promise_index];
		if (write->get_node()->get_promise(i)) {
			ModelAction *read = promise->get_action();
			if (read->is_rmw()) {
				mo_graph->addRMWEdge(write, read);
			}
			read_from(read, write);
			//First fix up the modification order for actions that happened
			//before the read
			r_modification_order(read, write);
			//Next fix up the modification order for actions that happened
			//after the read.
			post_r_modification_order(read, write);
			//Make sure the promise's value matches the write's value
			ASSERT(promise->get_value() == write->get_value());
			delete(promise);

			promises->erase(promises->begin() + promise_index);
			threads_to_check.push_back(read->get_tid());

			resolved = true;
		} else
			promise_index++;
	}

	//Check whether reading these writes has made threads unable to
	//resolve promises

	for (unsigned int i = 0; i < threads_to_check.size(); i++)
		mo_check_promises(threads_to_check[i], write);

	return resolved;
}

/**
 * Compute the set of promises that could potentially be satisfied by this
 * action. Note that the set computation actually appears in the Node, not in
 * ModelChecker.
 * @param curr The ModelAction that may satisfy promises
 */
void ModelChecker::compute_promises(ModelAction *curr)
{
	for (unsigned int i = 0; i < promises->size(); i++) {
		Promise *promise = (*promises)[i];
		const ModelAction *act = promise->get_action();
		if (!act->happens_before(curr) &&
				act->is_read() &&
				!act->could_synchronize_with(curr) &&
				!act->same_thread(curr) &&
				act->get_location() == curr->get_location() &&
				promise->get_value() == curr->get_value()) {
			curr->get_node()->set_promise(i, act->is_rmw());
		}
	}
}

/** Checks promises in response to change in ClockVector Threads. */
void ModelChecker::check_promises(thread_id_t tid, ClockVector *old_cv, ClockVector *merge_cv)
{
	for (unsigned int i = 0; i < promises->size(); i++) {
		Promise *promise = (*promises)[i];
		const ModelAction *act = promise->get_action();
		if ((old_cv == NULL || !old_cv->synchronized_since(act)) &&
				merge_cv->synchronized_since(act)) {
			if (promise->increment_threads(tid)) {
				//Promise has failed
				priv->failed_promise = true;
				return;
			}
		}
	}
}

void ModelChecker::check_promises_thread_disabled() {
	for (unsigned int i = 0; i < promises->size(); i++) {
		Promise *promise = (*promises)[i];
		if (promise->check_promise()) {
			priv->failed_promise = true;
			return;
		}
	}
}

/** Checks promises in response to addition to modification order for threads.
 * Definitions:
 * pthread is the thread that performed the read that created the promise
 *
 * pread is the read that created the promise
 *
 * pwrite is either the first write to same location as pread by
 * pthread that is sequenced after pread or the value read by the
 * first read to the same lcoation as pread by pthread that is
 * sequenced after pread..
 *
 *	1. If tid=pthread, then we check what other threads are reachable
 * through the mode order starting with pwrite.  Those threads cannot
 * perform a write that will resolve the promise due to modification
 * order constraints.
 *
 * 2. If the tid is not pthread, we check whether pwrite can reach the
 * action write through the modification order.  If so, that thread
 * cannot perform a future write that will resolve the promise due to
 * modificatin order constraints.
 *
 *	@param tid The thread that either read from the model action
 *	write, or actually did the model action write.
 *
 *	@param write The ModelAction representing the relevant write.
 */
void ModelChecker::mo_check_promises(thread_id_t tid, const ModelAction *write)
{
	void *location = write->get_location();
	for (unsigned int i = 0; i < promises->size(); i++) {
		Promise *promise = (*promises)[i];
		const ModelAction *act = promise->get_action();

		//Is this promise on the same location?
		if (act->get_location() != location)
			continue;

		//same thread as the promise
		if (act->get_tid() == tid) {

			//do we have a pwrite for the promise, if not, set it
			if (promise->get_write() == NULL) {
				promise->set_write(write);
				//The pwrite cannot happen before the promise
				if (write->happens_before(act) && (write != act)) {
					priv->failed_promise = true;
					return;
				}
			}
			if (mo_graph->checkPromise(write, promise)) {
				priv->failed_promise = true;
				return;
			}
		}

		//Don't do any lookups twice for the same thread
		if (promise->has_sync_thread(tid))
			continue;

		if (promise->get_write() && mo_graph->checkReachable(promise->get_write(), write)) {
			if (promise->increment_threads(tid)) {
				priv->failed_promise = true;
				return;
			}
		}
	}
}

/**
 * Compute the set of writes that may break the current pending release
 * sequence. This information is extracted from previou release sequence
 * calculations.
 *
 * @param curr The current ModelAction. Must be a release sequence fixup
 * action.
 */
void ModelChecker::compute_relseq_breakwrites(ModelAction *curr)
{
	if (pending_rel_seqs->empty())
		return;

	struct release_seq *pending = pending_rel_seqs->back();
	for (unsigned int i = 0; i < pending->writes.size(); i++) {
		const ModelAction *write = pending->writes[i];
		curr->get_node()->add_relseq_break(write);
	}

	/* NULL means don't break the sequence; just synchronize */
	curr->get_node()->add_relseq_break(NULL);
}

/**
 * Build up an initial set of all past writes that this 'read' action may read
 * from. This set is determined by the clock vector's "happens before"
 * relationship.
 * @param curr is the current ModelAction that we are exploring; it must be a
 * 'read' operation.
 */
void ModelChecker::build_reads_from_past(ModelAction *curr)
{
	std::vector<action_list_t> *thrd_lists = get_safe_ptr_vect_action(obj_thrd_map, curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	ModelAction *last_sc_write = NULL;

	if (curr->is_seqcst())
		last_sc_write = get_last_seq_cst_write(curr);

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Only consider 'write' actions */
			if (!act->is_write() || act == curr)
				continue;

			/* Don't consider more than one seq_cst write if we are a seq_cst read. */
			bool allow_read = true;

			if (curr->is_seqcst() && (act->is_seqcst() || (last_sc_write != NULL && act->happens_before(last_sc_write))) && act != last_sc_write)
				allow_read = false;
			else if (curr->get_sleep_flag() && !curr->is_seqcst() && !sleep_can_read_from(curr, act))
				allow_read = false;

			if (allow_read)
				curr->get_node()->add_read_from(act);

			/* Include at most one act per-thread that "happens before" curr */
			if (act->happens_before(curr))
				break;
		}
	}

	if (DBG_ENABLED()) {
		model_print("Reached read action:\n");
		curr->print();
		model_print("Printing may_read_from\n");
		curr->get_node()->print_may_read_from();
		model_print("End printing may_read_from\n");
	}
}

bool ModelChecker::sleep_can_read_from(ModelAction *curr, const ModelAction *write)
{
	while (true) {
		/* UNINIT actions don't have a Node, and they never sleep */
		if (write->is_uninitialized())
			return true;
		Node *prevnode = write->get_node()->get_parent();

		bool thread_sleep = prevnode->enabled_status(curr->get_tid()) == THREAD_SLEEP_SET;
		if (write->is_release() && thread_sleep)
			return true;
		if (!write->is_rmw()) {
			return false;
		}
		if (write->get_reads_from() == NULL)
			return true;
		write = write->get_reads_from();
	}
}

/**
 * @brief Create a new action representing an uninitialized atomic
 * @param location The memory location of the atomic object
 * @return A pointer to a new ModelAction
 */
ModelAction * ModelChecker::new_uninitialized_action(void *location) const
{
	ModelAction *act = (ModelAction *)snapshot_malloc(sizeof(class ModelAction));
	act = new (act) ModelAction(ATOMIC_UNINIT, std::memory_order_relaxed, location, 0, model_thread);
	act->create_cv(NULL);
	return act;
}

static void print_list(action_list_t *list, int exec_num = -1)
{
	action_list_t::iterator it;

	model_print("---------------------------------------------------------------------\n");
	if (exec_num >= 0)
		model_print("Execution %d:\n", exec_num);

	unsigned int hash = 0;

	for (it = list->begin(); it != list->end(); it++) {
		(*it)->print();
		hash = hash^(hash<<3)^((*it)->hash());
	}
	model_print("HASH %u\n", hash);
	model_print("---------------------------------------------------------------------\n");
}

#if SUPPORT_MOD_ORDER_DUMP
void ModelChecker::dumpGraph(char *filename) const
{
	char buffer[200];
	sprintf(buffer, "%s.dot", filename);
	FILE *file = fopen(buffer, "w");
	fprintf(file, "digraph %s {\n", filename);
	mo_graph->dumpNodes(file);
	ModelAction **thread_array = (ModelAction **)model_calloc(1, sizeof(ModelAction *) * get_num_threads());

	for (action_list_t::iterator it = action_trace->begin(); it != action_trace->end(); it++) {
		ModelAction *action = *it;
		if (action->is_read()) {
			fprintf(file, "N%u [label=\"%u, T%u\"];\n", action->get_seq_number(), action->get_seq_number(), action->get_tid());
			if (action->get_reads_from() != NULL)
				fprintf(file, "N%u -> N%u[label=\"rf\", color=red];\n", action->get_seq_number(), action->get_reads_from()->get_seq_number());
		}
		if (thread_array[action->get_tid()] != NULL) {
			fprintf(file, "N%u -> N%u[label=\"sb\", color=blue];\n", thread_array[action->get_tid()]->get_seq_number(), action->get_seq_number());
		}

		thread_array[action->get_tid()] = action;
	}
	fprintf(file, "}\n");
	model_free(thread_array);
	fclose(file);
}
#endif

/** @brief Prints an execution trace summary. */
void ModelChecker::print_summary() const
{
#if SUPPORT_MOD_ORDER_DUMP
	scheduler->print();
	char buffername[100];
	sprintf(buffername, "exec%04u", stats.num_total);
	mo_graph->dumpGraphToFile(buffername);
	sprintf(buffername, "graph%04u", stats.num_total);
	dumpGraph(buffername);
#endif

	if (!isfeasibleprefix())
		print_infeasibility("INFEASIBLE EXECUTION");
	print_list(action_trace, stats.num_total);
	model_print("\n");
}

/**
 * Add a Thread to the system for the first time. Should only be called once
 * per thread.
 * @param t The Thread to add
 */
void ModelChecker::add_thread(Thread *t)
{
	thread_map->put(id_to_int(t->get_id()), t);
	scheduler->add_thread(t);
}

/**
 * Removes a thread from the scheduler.
 * @param the thread to remove.
 */
void ModelChecker::remove_thread(Thread *t)
{
	scheduler->remove_thread(t);
}

/**
 * @brief Get a Thread reference by its ID
 * @param tid The Thread's ID
 * @return A Thread reference
 */
Thread * ModelChecker::get_thread(thread_id_t tid) const
{
	return thread_map->get(id_to_int(tid));
}

/**
 * @brief Get a reference to the Thread in which a ModelAction was executed
 * @param act The ModelAction
 * @return A Thread reference
 */
Thread * ModelChecker::get_thread(ModelAction *act) const
{
	return get_thread(act->get_tid());
}

/**
 * @brief Check if a Thread is currently enabled
 * @param t The Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelChecker::is_enabled(Thread *t) const
{
	return scheduler->is_enabled(t);
}

/**
 * @brief Check if a Thread is currently enabled
 * @param tid The ID of the Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelChecker::is_enabled(thread_id_t tid) const
{
	return scheduler->is_enabled(tid);
}

/**
 * Switch from a user-context to the "master thread" context (a.k.a. system
 * context). This switch is made with the intention of exploring a particular
 * model-checking action (described by a ModelAction object). Must be called
 * from a user-thread context.
 *
 * @param act The current action that will be explored. May be NULL only if
 * trace is exiting via an assertion (see ModelChecker::set_assert and
 * ModelChecker::has_asserted).
 * @return Return the value returned by the current action
 */
uint64_t ModelChecker::switch_to_master(ModelAction *act)
{
	DBG();
	Thread *old = thread_current();
	set_current_action(act);
	old->set_state(THREAD_READY);
	if (Thread::swap(old, &system_context) < 0) {
		perror("swap threads");
		exit(EXIT_FAILURE);
	}
	return old->get_return_value();
}

/**
 * Takes the next step in the execution, if possible.
 * @param curr The current step to take
 * @return Returns true (success) if a step was taken and false otherwise.
 */
bool ModelChecker::take_step(ModelAction *curr)
{
	if (has_asserted())
		return false;

	Thread *curr_thrd = get_thread(curr);
	ASSERT(curr_thrd->get_state() == THREAD_READY);

	curr = check_current_action(curr);

	/* Infeasible -> don't take any more steps */
	if (is_infeasible())
		return false;
	else if (isfeasibleprefix() && have_bug_reports()) {
		set_assert();
		return false;
	}

	if (params.bound != 0)
		if (priv->used_sequence_numbers > params.bound)
			return false;

	if (curr_thrd->is_blocked() || curr_thrd->is_complete())
		scheduler->remove_thread(curr_thrd);

	Thread *next_thrd = get_next_thread(curr);
	next_thrd = scheduler->next_thread(next_thrd);

	DEBUG("(%d, %d)\n", curr_thrd ? id_to_int(curr_thrd->get_id()) : -1,
			next_thrd ? id_to_int(next_thrd->get_id()) : -1);

	/*
	 * Launch end-of-execution release sequence fixups only when there are:
	 *
	 * (1) no more user threads to run (or when execution replay chooses
	 *     the 'model_thread')
	 * (2) pending release sequences
	 * (3) pending assertions (i.e., data races)
	 * (4) no pending promises
	 */
	if (!pending_rel_seqs->empty() && (!next_thrd || next_thrd->is_model_thread()) &&
			is_feasible_prefix_ignore_relseq() && !unrealizedraces.empty()) {
		model_print("*** WARNING: release sequence fixup action (%zu pending release seuqences) ***\n",
				pending_rel_seqs->size());
		ModelAction *fixup = new ModelAction(MODEL_FIXUP_RELSEQ,
				std::memory_order_seq_cst, NULL, VALUE_NONE,
				model_thread);
		set_current_action(fixup);
		return true;
	}

	/* next_thrd == NULL -> don't take any more steps */
	if (!next_thrd)
		return false;

	next_thrd->set_state(THREAD_RUNNING);

	if (next_thrd->get_pending() != NULL) {
		/* restart a pending action */
		set_current_action(next_thrd->get_pending());
		next_thrd->set_pending(NULL);
		next_thrd->set_state(THREAD_READY);
		return true;
	}

	/* Return false only if swap fails with an error */
	return (Thread::swap(&system_context, next_thrd) == 0);
}

/** Wrapper to run the user's main function, with appropriate arguments */
void user_main_wrapper(void *)
{
	user_main(model->params.argc, model->params.argv);
}

/** @brief Run ModelChecker for the user program */
void ModelChecker::run()
{
	do {
		thrd_t user_thread;
		Thread *t = new Thread(&user_thread, &user_main_wrapper, NULL);

		add_thread(t);

		/* Run user thread up to its first action */
		scheduler->next_thread(t);
		Thread::swap(&system_context, t);

		/* Wait for all threads to complete */
		while (take_step(priv->current_action));
	} while (next_execution());

	print_stats();
}
