#include <stdio.h>

#include "model.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "snapshot-interface.h"
#include "common.h"
#include "clockvector.h"

#define INITIAL_THREAD_ID	0

ModelChecker *model;

/** @brief Constructor */
ModelChecker::ModelChecker()
	:
	/* Initialize default scheduler */
	scheduler(new Scheduler()),
	/* First thread created will have id INITIAL_THREAD_ID */
	next_thread_id(INITIAL_THREAD_ID),
	used_sequence_numbers(0),

	num_executions(0),
	current_action(NULL),
	diverge(NULL),
	nextThread(THREAD_ID_T_NONE),
	action_trace(new action_list_t()),
	thread_map(new std::map<int, Thread *>),
	obj_map(new std::map<const void *, action_list_t>()),
	obj_thrd_map(new std::map<void *, std::vector<action_list_t> >()),
	thrd_last_action(new std::vector<ModelAction *>(1)),
	node_stack(new NodeStack()),
	next_backtrack(NULL)
{
}

/** @brief Destructor */
ModelChecker::~ModelChecker()
{
	std::map<int, Thread *>::iterator it;
	for (it = thread_map->begin(); it != thread_map->end(); it++)
		delete (*it).second;
	delete thread_map;

	delete obj_thrd_map;
	delete obj_map;
	delete action_trace;
	delete thrd_last_action;
	delete node_stack;
	delete scheduler;
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
	t = (*thread_map)[id_to_int(nextThread)];

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
	ModelAction *next;
	thread_id_t tid;

	/* Have we completed exploring the preselected path? */
	if (diverge == NULL)
		return THREAD_ID_T_NONE;

	/* Else, we are trying to replay an execution */
	next = node_stack->get_next()->get_action();

	if (next == diverge) {
		Node *node = next->get_node()->get_parent();

		/* Reached divergence point */
		DEBUG("*** Divergence point ***\n");
		tid = node->get_next_backtrack();
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
	action_list_t *list = &(*obj_map)[act->get_location()];
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

ModelAction * ModelChecker::get_next_backtrack()
{
	ModelAction *next = next_backtrack;
	next_backtrack = NULL;
	return next;
}

void ModelChecker::check_current_action(void)
{
	Node *currnode;

	ModelAction *curr = this->current_action;
	ModelAction *tmp;
	current_action = NULL;
	if (!curr) {
		DEBUG("trying to push NULL action...\n");
		return;
	}

	tmp = node_stack->explore_action(curr);
	if (tmp) {
		/* Discard duplicate ModelAction; use action from NodeStack */
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
	}

	/* Assign 'creation' parent */
	if (curr->get_type() == THREAD_CREATE) {
		Thread *th = (Thread *)curr->get_location();
		th->set_creation(curr);
	}

	nextThread = get_next_replay_thread();

	currnode = curr->get_node()->get_parent();

	if (!currnode->backtrack_empty())
		if (!next_backtrack || *curr > *next_backtrack)
			next_backtrack = curr;

	set_backtracking(curr);

	add_action_to_lists(curr);

	/* Assign reads_from values */
	/* TODO: perform release/acquire synchronization here; include
	 * reads_from as ModelAction member? */
	Thread *th = get_thread(curr->get_tid());
	uint64_t value = VALUE_NONE;
	if (curr->is_read()) {
		const ModelAction *reads_from = curr->get_node()->get_next_read_from();
		value = reads_from->get_value();
		/* Assign reads_from, perform release/acquire synchronization */
		curr->read_from(reads_from);
	}
	th->set_return_value(value);
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

	(*obj_map)[act->get_location()].push_back(act);

	std::vector<action_list_t> *vec = &(*obj_thrd_map)[act->get_location()];
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
	action_list_t *list = &(*obj_map)[location];
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

ClockVector * ModelChecker::get_cv(thread_id_t tid) {
	return get_parent_action(tid)->get_cv();
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
	std::vector<action_list_t> *thrd_lists = &(*obj_thrd_map)[curr->get_location()];
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

			/* Don't consider more than one seq_cst write */
			if (!act->is_seqcst() || act == last_seq_cst) {
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
		/* TODO: need a more informative way of reporting errors */
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

void ModelChecker::print_summary(void)
{
	printf("\n");
	printf("Number of executions: %d\n", num_executions);
	printf("Total nodes created: %d\n", node_stack->get_total_nodes());

	scheduler->print();

	print_list(action_trace);
	printf("\n");
}

int ModelChecker::add_thread(Thread *t)
{
	(*thread_map)[id_to_int(t->get_id())] = t;
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
	Thread *old;

	DBG();
	old = thread_current();
	set_current_action(act);
	old->set_state(THREAD_READY);
	return Thread::swap(old, get_system_context());
}
