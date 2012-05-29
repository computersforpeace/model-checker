#include <stdio.h>

#include "model.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "snapshot-interface.h"
#include "common.h"

#define INITIAL_THREAD_ID	0

ModelChecker *model;

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
	thread_map(new std::map<int, class Thread *>),
	obj_thrd_map(new std::map<void *, std::vector<action_list_t> >()),
	thrd_last_action(new std::vector<ModelAction *>(1)),
	node_stack(new NodeStack()),
	next_backtrack(NULL)
{
}

ModelChecker::~ModelChecker()
{
	std::map<int, class Thread *>::iterator it;
	for (it = thread_map->begin(); it != thread_map->end(); it++)
		delete (*it).second;
	delete thread_map;

	delete obj_thrd_map;
	delete action_trace;
	delete thrd_last_action;
	delete node_stack;
	delete scheduler;
}

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

thread_id_t ModelChecker::get_next_id()
{
	return next_thread_id++;
}

int ModelChecker::get_num_threads()
{
	return next_thread_id;
}

int ModelChecker::get_next_seq_num()
{
	return ++used_sequence_numbers;
}

Thread * ModelChecker::schedule_next_thread()
{
	Thread *t;
	if (nextThread == THREAD_ID_T_NONE)
		return NULL;
	t = (*thread_map)[id_to_int(nextThread)];

	ASSERT(t != NULL);

	return t;
}

/*
 * get_next_replay_thread() - Choose the next thread in the replay sequence
 *
 * If we've reached the 'diverge' point, then we pick a thread from the
 *   backtracking set.
 * Otherwise, we simply return the next thread in the sequence.
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
		Node *node = next->get_node();

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
		case THREAD_CREATE:
		case THREAD_YIELD:
		case THREAD_JOIN:
			return NULL;
		case ATOMIC_READ:
		case ATOMIC_WRITE:
		default:
			break;
	}
	/* linear search: from most recent to oldest */
	action_list_t::reverse_iterator rit;
	for (rit = action_trace->rbegin(); rit != action_trace->rend(); rit++) {
		ModelAction *prev = *rit;
		if (act->is_dependent(prev))
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

	node = prev->get_node();

	while (!node->is_enabled(t))
		t = t->get_parent();

	/* Check if this has been explored already */
	if (node->has_been_explored(t->get_id()))
		return;

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
	current_action = NULL;
	if (!curr) {
		DEBUG("trying to push NULL action...\n");
		return;
	}

	curr = node_stack->explore_action(curr, get_parent_action(curr->get_tid()));

	/* Assign 'creation' parent */
	if (curr->get_type() == THREAD_CREATE) {
		Thread *th = (Thread *)curr->get_location();
		th->set_creation(curr);
	}

	nextThread = get_next_replay_thread();

	currnode = curr->get_node();

	if (!currnode->backtrack_empty())
		if (!next_backtrack || *curr > *next_backtrack)
			next_backtrack = curr;

	set_backtracking(curr);

	add_action_to_lists(curr);
}

void ModelChecker::add_action_to_lists(ModelAction *act)
{
	action_trace->push_back(act);

	std::vector<action_list_t> *vec = &(*obj_thrd_map)[act->get_location()];
	if (id_to_int(act->get_tid()) >= (int)vec->size())
		vec->resize(next_thread_id);
	(*vec)[id_to_int(act->get_tid())].push_back(act);

	(*thrd_last_action)[id_to_int(act->get_tid())] = act;
}

ModelAction * ModelChecker::get_last_action(thread_id_t tid)
{
	int nthreads = get_num_threads();
	if ((int)thrd_last_action->size() < nthreads)
		thrd_last_action->resize(nthreads);
	return (*thrd_last_action)[id_to_int(tid)];
}

ModelAction * ModelChecker::get_parent_action(thread_id_t tid)
{
	ModelAction *parent = get_last_action(tid);
	if (!parent)
		parent = get_thread(tid)->get_creation();
	return parent;
}

void ModelChecker::print_summary(void)
{
	printf("\n");
	printf("Number of executions: %d\n", num_executions);
	printf("Total nodes created: %d\n", Node::get_total_nodes());

	scheduler->print();

	print_list(action_trace);
	printf("\n");
}

void ModelChecker::print_list(action_list_t *list)
{
	action_list_t::iterator it;

	printf("---------------------------------------------------------------------\n");
	printf("Trace:\n");

	for (it = list->begin(); it != list->end(); it++) {
		(*it)->print();
	}
	printf("---------------------------------------------------------------------\n");
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

int ModelChecker::switch_to_master(ModelAction *act)
{
	Thread *old;

	DBG();
	old = thread_current();
	set_current_action(act);
	old->set_state(THREAD_READY);
	return Thread::swap(old, get_system_context());
}
