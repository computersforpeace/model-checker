#include <stdio.h>

#include "model.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "snapshot-interface.h"
#include "common.h"
#include "clockvector.h"
#include "cyclegraph.h"
#include "promise.h"

#define INITIAL_THREAD_ID	0

ModelChecker *model;

/** @brief Constructor */
ModelChecker::ModelChecker(struct model_params params) :
	/* Initialize default scheduler */
	scheduler(new Scheduler()),
	/* First thread created will have id INITIAL_THREAD_ID */
	next_thread_id(INITIAL_THREAD_ID),
	used_sequence_numbers(0),
	num_executions(0),
	params(params),
	current_action(NULL),
	diverge(NULL),
	nextThread(THREAD_ID_T_NONE),
	action_trace(new action_list_t()),
	thread_map(new HashTable<int, Thread *, int>()),
	obj_map(new HashTable<const void *, action_list_t, uintptr_t, 4>()),
	obj_thrd_map(new HashTable<void *, std::vector<action_list_t>, uintptr_t, 4 >()),
	promises(new std::vector<Promise *>()),
	thrd_last_action(new std::vector<ModelAction *>(1)),
	node_stack(new NodeStack()),
	next_backtrack(NULL),
	cyclegraph(new CycleGraph()),
	failed_promise(false)
{
}

/** @brief Destructor */
ModelChecker::~ModelChecker()
{
	for (int i = 0; i < get_num_threads(); i++)
		delete thread_map->get(i);
	delete thread_map;

	delete obj_thrd_map;
	delete obj_map;
	delete action_trace;
	delete thrd_last_action;
	delete node_stack;
	delete scheduler;
	delete cyclegraph;
}

/**
 * Restores user program to initial state and resets all model-checker data
 * structures.
 */
void ModelChecker::reset_to_initial_state()
{
	DEBUG("+++ Resetting to initial state +++\n");
	node_stack->reset_execution();
	current_action = NULL;
	next_thread_id = INITIAL_THREAD_ID;
	used_sequence_numbers = 0;
	nextThread = 0;
	next_backtrack = NULL;
	failed_promise = false;
	snapshotObject->backTrackBeforeStep(0);
}

/** @returns a thread ID for a new Thread */
thread_id_t ModelChecker::get_next_id()
{
	return next_thread_id++;
}

/** @returns the number of user threads created during this execution */
int ModelChecker::get_num_threads()
{
	return next_thread_id;
}

/** @returns a sequence number for a new ModelAction */
modelclock_t ModelChecker::get_next_seq_num()
{
	return ++used_sequence_numbers;
}

/**
 * Performs the "scheduling" for the model-checker. That is, it checks if the
 * model-checker has selected a "next thread to run" and returns it, if
 * available. This function should be called from the Scheduler routine, where
 * the Scheduler falls back to a default scheduling routine if needed.
 *
 * @return The next thread chosen by the model-checker. If the model-checker
 * makes no selection, retuns NULL.
 */
Thread * ModelChecker::schedule_next_thread()
{
	Thread *t;
	if (nextThread == THREAD_ID_T_NONE)
		return NULL;
	t = thread_map->get(id_to_int(nextThread));

	ASSERT(t != NULL);

	return t;
}

/**
 * Choose the next thread in the replay sequence.
 *
 * If the replay sequence has reached the 'diverge' point, returns a thread
 * from the backtracking set. Otherwise, simply returns the next thread in the
 * sequence that is being replayed.
 */
thread_id_t ModelChecker::get_next_replay_thread()
{
	thread_id_t tid;

	/* Have we completed exploring the preselected path? */
	if (diverge == NULL)
		return THREAD_ID_T_NONE;

	/* Else, we are trying to replay an execution */
	ModelAction * next = node_stack->get_next()->get_action();

	if (next == diverge) {
		Node *nextnode = next->get_node();
		/* Reached divergence point */
		if (nextnode->increment_promise()) {
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
		} else {
			/* Make a different thread execute for next step */
			Node *node = nextnode->get_parent();
			tid = node->get_next_backtrack();
			node_stack->pop_restofstack(1);
		}
		DEBUG("*** Divergence point ***\n");
		diverge = NULL;
	} else {
		tid = next->get_tid();
	}
	DEBUG("*** ModelChecker chose next thread = %d ***\n", tid);
	return tid;
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

	num_executions++;

	if (isfinalfeasible() || DBG_ENABLED())
		print_summary();

	if ((diverge = model->get_next_backtrack()) == NULL)
		return false;

	if (DBG_ENABLED()) {
		printf("Next execution will diverge at:\n");
		diverge->print();
	}

	model->reset_to_initial_state();
	return true;
}

ModelAction * ModelChecker::get_last_conflict(ModelAction *act)
{
	action_type type = act->get_type();

	switch (type) {
		case ATOMIC_READ:
		case ATOMIC_WRITE:
		case ATOMIC_RMW:
			break;
		default:
			return NULL;
	}
	/* linear search: from most recent to oldest */
	action_list_t *list = obj_map->ensureptr(act->get_location());
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin(); rit != list->rend(); rit++) {
		ModelAction *prev = *rit;
		if (act->is_synchronizing(prev))
			return prev;
	}
	return NULL;
}

void ModelChecker::set_backtracking(ModelAction *act)
{
	ModelAction *prev;
	Node *node;
	Thread *t = get_thread(act->get_tid());

	prev = get_last_conflict(act);
	if (prev == NULL)
		return;

	node = prev->get_node()->get_parent();

	while (!node->is_enabled(t))
		t = t->get_parent();

	/* Check if this has been explored already */
	if (node->has_been_explored(t->get_id()))
		return;

	/* Cache the latest backtracking point */
	if (!next_backtrack || *prev > *next_backtrack)
		next_backtrack = prev;

	/* If this is a new backtracking point, mark the tree */
	if (!node->set_backtrack(t->get_id()))
		return;
	DEBUG("Setting backtrack: conflict = %d, instead tid = %d\n",
			prev->get_tid(), t->get_id());
	if (DBG_ENABLED()) {
		prev->print();
		act->print();
	}
}

/**
 * Returns last backtracking point. The model checker will explore a different
 * path for this point in the next execution.
 * @return The ModelAction at which the next execution should diverge.
 */
ModelAction * ModelChecker::get_next_backtrack()
{
	ModelAction *next = next_backtrack;
	next_backtrack = NULL;
	return next;
}

void ModelChecker::check_current_action(void)
{
	ModelAction *curr = this->current_action;
	bool already_added = false;
	this->current_action = NULL;
	if (!curr) {
		DEBUG("trying to push NULL action...\n");
		return;
	}

	if (curr->is_rmwc() || curr->is_rmw()) {
		ModelAction *tmp = process_rmw(curr);
		already_added = true;
		delete curr;
		curr = tmp;
	} else {
		ModelAction * tmp = node_stack->explore_action(curr);
		if (tmp) {
			/* Discard duplicate ModelAction; use action from NodeStack */
			/* First restore type and order in case of RMW operation */
			if (curr->is_rmwr())
				tmp->copy_typeandorder(curr);

			/* If we have diverged, we need to reset the clock vector. */
			if (diverge == NULL)
				tmp->create_cv(get_parent_action(tmp->get_tid()));

			delete curr;
			curr = tmp;
		} else {
			/*
			 * Perform one-time actions when pushing new ModelAction onto
			 * NodeStack
			 */
			curr->create_cv(get_parent_action(curr->get_tid()));
			/* Build may_read_from set */
			if (curr->is_read())
				build_reads_from_past(curr);
			if (curr->is_write())
				compute_promises(curr);
		}
	}

	/* Assign 'creation' parent */
	if (curr->get_type() == THREAD_CREATE) {
		Thread *th = (Thread *)curr->get_location();
		th->set_creation(curr);
	}

	/* Deal with new thread */
	if (curr->get_type() == THREAD_START)
		check_promises(NULL, curr->get_cv());

	/* Assign reads_from values */
	Thread *th = get_thread(curr->get_tid());
	uint64_t value = VALUE_NONE;
	if (curr->is_read()) {
		const ModelAction *reads_from = curr->get_node()->get_read_from();
		if (reads_from != NULL) {
			value = reads_from->get_value();
			/* Assign reads_from, perform release/acquire synchronization */
			curr->read_from(reads_from);
			r_modification_order(curr,reads_from);
		} else {
			/* Read from future value */
			value = curr->get_node()->get_future_value();
			curr->read_from(NULL);
			Promise * valuepromise = new Promise(curr, value);
			promises->push_back(valuepromise);
		}
	} else if (curr->is_write()) {
		w_modification_order(curr);
		resolve_promises(curr);
	}

	th->set_return_value(value);

	/* Add action to list.  */
	if (!already_added)
		add_action_to_lists(curr);

	/** @todo Is there a better interface for setting the next thread rather
		 than this field/convoluted approach?  Perhaps like just returning
		 it or something? */

	/* Do not split atomic actions. */
	if (curr->is_rmwr())
		nextThread = thread_current()->get_id();
	else
		nextThread = get_next_replay_thread();

	Node *currnode = curr->get_node();
	Node *parnode = currnode->get_parent();

	if (!parnode->backtrack_empty() || !currnode->read_from_empty() ||
	          !currnode->future_value_empty() || !currnode->promise_empty())
		if (!next_backtrack || *curr > *next_backtrack)
			next_backtrack = curr;

	set_backtracking(curr);
}

/** @returns whether the current partial trace is feasible. */
bool ModelChecker::isfeasible() {
	return !cyclegraph->checkForCycles() && !failed_promise;
}

/** Returns whether the current completed trace is feasible. */
bool ModelChecker::isfinalfeasible() {
	return isfeasible() && promises->size() == 0;
}

/** Close out a RMWR by converting previous RMWR into a RMW or READ. */
ModelAction * ModelChecker::process_rmw(ModelAction * act) {
	int tid = id_to_int(act->get_tid());
	ModelAction *lastread = get_last_action(tid);
	lastread->process_rmw(act);
	if (act->is_rmw())
		cyclegraph->addRMWEdge(lastread, lastread->get_reads_from());
	return lastread;
}

/**
 * Updates the cyclegraph with the constraints imposed from the current read.
 * @param curr The current action. Must be a read.
 * @param rf The action that curr reads from. Must be a write.
 */
void ModelChecker::r_modification_order(ModelAction * curr, const ModelAction *rf) {
	std::vector<action_list_t> *thrd_lists = obj_thrd_map->ensureptr(curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Include at most one act per-thread that "happens before" curr */
			if (act->happens_before(curr)) {
				if (act->is_read()) {
					const ModelAction * prevreadfrom = act->get_reads_from();
					if (prevreadfrom != NULL && rf != prevreadfrom)
						cyclegraph->addEdge(rf, prevreadfrom);
				} else if (rf != act) {
					cyclegraph->addEdge(rf, act);
				}
				break;
			}
		}
	}
}

/** Updates the cyclegraph with the constraints imposed from the
 *  current read. */
void ModelChecker::post_r_modification_order(ModelAction * curr, const ModelAction *rf) {
	std::vector<action_list_t> *thrd_lists = obj_thrd_map->ensureptr(curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		ModelAction *lastact = NULL;

		/* Find last action that happens after curr */
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;
			if (curr->happens_before(act)) {
				lastact = act;
			} else
				break;
		}

			/* Include at most one act per-thread that "happens before" curr */
		if (lastact != NULL) {
			if (lastact->is_read()) {
				const ModelAction * postreadfrom = lastact->get_reads_from();
				if (postreadfrom != NULL&&rf != postreadfrom)
					cyclegraph->addEdge(postreadfrom, rf);
			} else if (rf != lastact) {
				cyclegraph->addEdge(lastact, rf);
			}
			break;
		}
	}
}

/**
 * Updates the cyclegraph with the constraints imposed from the current write.
 * @param curr The current action. Must be a write.
 */
void ModelChecker::w_modification_order(ModelAction * curr) {
	std::vector<action_list_t> *thrd_lists = obj_thrd_map->ensureptr(curr->get_location());
	unsigned int i;
	ASSERT(curr->is_write());

	if (curr->is_seqcst()) {
		/* We have to at least see the last sequentially consistent write,
			 so we are initialized. */
		ModelAction * last_seq_cst = get_last_seq_cst(curr->get_location());
		if (last_seq_cst != NULL)
			cyclegraph->addEdge(curr, last_seq_cst);
	}

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Include at most one act per-thread that "happens before" curr */
			if (act->happens_before(curr)) {
				if (act->is_read()) {
					cyclegraph->addEdge(curr, act->get_reads_from());
				} else
					cyclegraph->addEdge(curr, act);
				break;
			} else {
				if (act->is_read()&&!act->is_synchronizing(curr)&&!act->same_thread(curr)) {
					/* We have an action that:
						 (1) did not happen before us
						 (2) is a read and we are a write
						 (3) cannot synchronize with us
						 (4) is in a different thread
						 =>
						 that read could potentially read from our write.
					*/
					if (act->get_node()->add_future_value(curr->get_value())&&
							(!next_backtrack || *act > * next_backtrack))
						next_backtrack = act;
				}
			}
		}
	}
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
	action_trace->push_back(act);

	obj_map->ensureptr(act->get_location())->push_back(act);

	std::vector<action_list_t> *vec = obj_thrd_map->ensureptr(act->get_location());
	if (tid >= (int)vec->size())
		vec->resize(next_thread_id);
	(*vec)[tid].push_back(act);

	if ((int)thrd_last_action->size() <= tid)
		thrd_last_action->resize(get_num_threads());
	(*thrd_last_action)[tid] = act;
}

ModelAction * ModelChecker::get_last_action(thread_id_t tid)
{
	int nthreads = get_num_threads();
	if ((int)thrd_last_action->size() < nthreads)
		thrd_last_action->resize(nthreads);
	return (*thrd_last_action)[id_to_int(tid)];
}

/**
 * Gets the last memory_order_seq_cst action (in the total global sequence)
 * performed on a particular object (i.e., memory location).
 * @param location The object location to check
 * @return The last seq_cst action performed
 */
ModelAction * ModelChecker::get_last_seq_cst(const void *location)
{
	action_list_t *list = obj_map->ensureptr(location);
	/* Find: max({i in dom(S) | seq_cst(t_i) && isWrite(t_i) && samevar(t_i, t)}) */
	action_list_t::reverse_iterator rit;
	for (rit = list->rbegin(); rit != list->rend(); rit++)
		if ((*rit)->is_write() && (*rit)->is_seqcst())
			return *rit;
	return NULL;
}

ModelAction * ModelChecker::get_parent_action(thread_id_t tid)
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
ClockVector * ModelChecker::get_cv(thread_id_t tid) {
	return get_parent_action(tid)->get_cv();
}


/** Resolve the given promises. */

void ModelChecker::resolve_promises(ModelAction *write) {
	for (unsigned int i = 0, promise_index = 0;promise_index<promises->size(); i++) {
		Promise * promise = (*promises)[promise_index];
		if (write->get_node()->get_promise(i)) {
			ModelAction * read = promise->get_action();
			read->read_from(write);
			r_modification_order(read, write);
			post_r_modification_order(read, write);
			promises->erase(promises->begin()+promise_index);
		} else
			promise_index++;
	}
}

/** Compute the set of promises that could potentially be satisfied by
 *  this action. */

void ModelChecker::compute_promises(ModelAction *curr) {
	for (unsigned int i = 0;i<promises->size();i++) {
		Promise * promise = (*promises)[i];
		const ModelAction * act = promise->get_action();
		if (!act->happens_before(curr)&&
				act->is_read()&&
				!act->is_synchronizing(curr)&&
				!act->same_thread(curr)&&
				promise->get_value() == curr->get_value()) {
			curr->get_node()->set_promise(i);
		}
	}
}

/** Checks promises in response to change in ClockVector Threads. */

void ModelChecker::check_promises(ClockVector *old_cv, ClockVector * merge_cv) {
	for (unsigned int i = 0;i<promises->size();i++) {
		Promise * promise = (*promises)[i];
		const ModelAction * act = promise->get_action();
		if ((old_cv == NULL||!old_cv->synchronized_since(act))&&
				merge_cv->synchronized_since(act)) {
			//This thread is no longer able to send values back to satisfy the promise
			int num_synchronized_threads = promise->increment_threads();
			if (num_synchronized_threads == model->get_num_threads()) {
				//Promise has failed
				failed_promise = true;
				return;
			}
		}
	}
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
	std::vector<action_list_t> *thrd_lists = obj_thrd_map->ensureptr(curr->get_location());
	unsigned int i;
	ASSERT(curr->is_read());

	ModelAction *last_seq_cst = NULL;

	/* Track whether this object has been initialized */
	bool initialized = false;

	if (curr->is_seqcst()) {
		last_seq_cst = get_last_seq_cst(curr->get_location());
		/* We have to at least see the last sequentially consistent write,
			 so we are initialized. */
		if (last_seq_cst != NULL)
			initialized = true;
	}

	/* Iterate over all threads */
	for (i = 0; i < thrd_lists->size(); i++) {
		/* Iterate over actions in thread, starting from most recent */
		action_list_t *list = &(*thrd_lists)[i];
		action_list_t::reverse_iterator rit;
		for (rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *act = *rit;

			/* Only consider 'write' actions */
			if (!act->is_write())
				continue;

			/* Don't consider more than one seq_cst write if we are a seq_cst read. */
			if (!act->is_seqcst() || !curr->is_seqcst() || act == last_seq_cst) {
				DEBUG("Adding action to may_read_from:\n");
				if (DBG_ENABLED()) {
					act->print();
					curr->print();
				}
				curr->get_node()->add_read_from(act);
			}

			/* Include at most one act per-thread that "happens before" curr */
			if (act->happens_before(curr)) {
				initialized = true;
				break;
			}
		}
	}

	if (!initialized) {
		/** @todo Need a more informative way of reporting errors. */
		printf("ERROR: may read from uninitialized atomic\n");
	}

	if (DBG_ENABLED() || !initialized) {
		printf("Reached read action:\n");
		curr->print();
		printf("Printing may_read_from\n");
		curr->get_node()->print_may_read_from();
		printf("End printing may_read_from\n");
	}

	ASSERT(initialized);
}

static void print_list(action_list_t *list)
{
	action_list_t::iterator it;

	printf("---------------------------------------------------------------------\n");
	printf("Trace:\n");

	for (it = list->begin(); it != list->end(); it++) {
		(*it)->print();
	}
	printf("---------------------------------------------------------------------\n");
}

void ModelChecker::print_summary()
{
	printf("\n");
	printf("Number of executions: %d\n", num_executions);
	printf("Total nodes created: %d\n", node_stack->get_total_nodes());

	scheduler->print();

	if (!isfinalfeasible())
		printf("INFEASIBLE EXECUTION!\n");
	print_list(action_trace);
	printf("\n");
}

int ModelChecker::add_thread(Thread *t)
{
	thread_map->put(id_to_int(t->get_id()), t);
	scheduler->add_thread(t);
	return 0;
}

void ModelChecker::remove_thread(Thread *t)
{
	scheduler->remove_thread(t);
}

/**
 * Switch from a user-context to the "master thread" context (a.k.a. system
 * context). This switch is made with the intention of exploring a particular
 * model-checking action (described by a ModelAction object). Must be called
 * from a user-thread context.
 * @param act The current action that will be explored. May be NULL, although
 * there is little reason to switch to the model-checker without an action to
 * explore (note: act == NULL is sometimes used as a hack to allow a thread to
 * yield control without performing any progress; see thrd_join()).
 * @return Return status from the 'swap' call (i.e., success/fail, 0/-1)
 */
int ModelChecker::switch_to_master(ModelAction *act)
{
	DBG();
	Thread * old = thread_current();
	set_current_action(act);
	old->set_state(THREAD_READY);
	return Thread::swap(old, get_system_context());
}
