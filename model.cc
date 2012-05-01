#include <stdio.h>

#include "model.h"
#include "action.h"
#include "tree.h"
#include "schedule.h"
#include "common.h"

#define INITIAL_THREAD_ID	0

ModelChecker *model;

ModelChecker::ModelChecker()
{
	/* First thread created will have id (INITIAL_THREAD_ID + 1) */
	this->used_thread_id = INITIAL_THREAD_ID;
	/* Initialize default scheduler */
	this->scheduler = new Scheduler();

	num_executions = 0;
	this->current_action = NULL;
	this->exploring = NULL;
	this->nextThread = THREAD_ID_T_NONE;

	rootNode = new TreeNode(NULL);
	currentNode = rootNode;
	action_trace = new action_list_t();
}

ModelChecker::~ModelChecker()
{
	delete action_trace;
	delete this->scheduler;
	delete rootNode;
}

void ModelChecker::reset_to_initial_state()
{
	DEBUG("+++ Resetting to initial state +++\n");
	std::map<thread_id_t, class Thread *>::iterator it;
	for (it = thread_map.begin(); it != thread_map.end(); it++)
		delete (*it).second;
	thread_map.clear();
	action_trace = new action_list_t();
	currentNode = rootNode;
	current_action = NULL;
	used_thread_id = INITIAL_THREAD_ID;
	/* scheduler reset ? */
}

thread_id_t ModelChecker::get_next_id()
{
	return ++used_thread_id;
}

Thread * ModelChecker::schedule_next_thread()
{
	Thread *t;
	if (nextThread == THREAD_ID_T_NONE)
		return NULL;
	t = thread_map[nextThread];
	if (t == NULL)
		DEBUG("*** error: thread not in thread_map: id = %d\n", nextThread);
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

	next = exploring->get_state();

	if (next == exploring->get_diverge()) {
		TreeNode *node = next->get_node();

		/* Reached divergence point; discard our current 'exploring' */
		DEBUG("*** Discard 'Backtrack' object ***\n");
		tid = node->getNextBacktrack();
		delete exploring;
		exploring = NULL;
	} else {
		tid = next->get_tid();
	}
	DEBUG("*** ModelChecker chose next thread = %d ***\n", tid);
	return tid;
}

thread_id_t ModelChecker::advance_backtracking_state()
{
	/* Have we completed exploring the preselected path? */
	if (exploring == NULL)
		return THREAD_ID_T_NONE;

	/* Else, we are trying to replay an execution */
	exploring->advance_state();
	if (exploring->get_state() == NULL)
		DEBUG("*** error: reached end of backtrack trace\n");

	return get_next_replay_thread();
}

bool ModelChecker::next_execution()
{
	num_executions++;
	print_summary();
	if ((exploring = model->get_next_backtrack()) == NULL)
		return false;
	model->reset_to_initial_state();
	nextThread = get_next_replay_thread();
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
	TreeNode *node;

	prev = get_last_conflict(act);
	if (prev == NULL)
		return;

	node = prev->get_node();

	/* Check if this has been explored already */
	if (node->hasBeenExplored(act->get_tid()))
		return;
	/* If this is a new backtracking point, mark the tree */
	if (node->setBacktrack(act->get_tid()) != 0)
		return;

	DEBUG("Setting backtrack: conflict = %d, instead tid = %d\n",
			prev->get_tid(), act->get_tid());
	if (DBG_ENABLED()) {
		prev->print();
		act->print();
	}

	Backtrack *back = new Backtrack(prev, action_trace);
	backtrack_list.push_back(back);
}

Backtrack * ModelChecker::get_next_backtrack()
{
	Backtrack *next;
	if (backtrack_list.empty())
		return NULL;
	next = backtrack_list.back();
	backtrack_list.pop_back();
	return next;
}

void ModelChecker::check_current_action(void)
{
	ModelAction *next = this->current_action;

	if (!next) {
		DEBUG("trying to push NULL action...\n");
		return;
	}
	current_action = NULL;
	nextThread = advance_backtracking_state();
	next->set_node(currentNode);
	set_backtracking(next);
	currentNode = currentNode->exploreChild(next->get_tid());
	this->action_trace->push_back(next);
}

void ModelChecker::print_summary(void)
{
	action_list_t::iterator it;

	printf("\n");
	printf("---------------------------------------------------------------------\n");
	printf("Number of executions: %d\n", num_executions);
	printf("Total nodes created: %d\n\n", TreeNode::getTotalNodes());

	scheduler->print();

	printf("Trace:\n\n");

	for (it = action_trace->begin(); it != action_trace->end(); it++) {
		DBG();
		(*it)->print();
	}
	printf("---------------------------------------------------------------------\n");
}

int ModelChecker::add_thread(Thread *t)
{
	thread_map[t->get_id()] = t;
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

ModelAction::ModelAction(action_type_t type, memory_order order, void *loc, int value)
{
	Thread *t = thread_current();
	ModelAction *act = this;

	act->type = type;
	act->order = order;
	act->location = loc;
	act->tid = t->get_id();
	act->value = value;
}

bool ModelAction::is_read()
{
	return type == ATOMIC_READ;
}

bool ModelAction::is_write()
{
	return type == ATOMIC_WRITE;
}

bool ModelAction::is_acquire()
{
	switch (order) {
	case memory_order_acquire:
	case memory_order_acq_rel:
	case memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::is_release()
{
	switch (order) {
	case memory_order_release:
	case memory_order_acq_rel:
	case memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::same_var(ModelAction *act)
{
	return location == act->location;
}

bool ModelAction::same_thread(ModelAction *act)
{
	return tid == act->tid;
}

bool ModelAction::is_dependent(ModelAction *act)
{
	if (!is_read() && !is_write())
		return false;
	if (!act->is_read() && !act->is_write())
		return false;
	if (same_var(act) && !same_thread(act) &&
			(is_write() || act->is_write()))
		return true;
	return false;
}

void ModelAction::print(void)
{
	const char *type_str;
	switch (this->type) {
	case THREAD_CREATE:
		type_str = "thread create";
		break;
	case THREAD_YIELD:
		type_str = "thread yield";
		break;
	case THREAD_JOIN:
		type_str = "thread join";
		break;
	case ATOMIC_READ:
		type_str = "atomic read";
		break;
	case ATOMIC_WRITE:
		type_str = "atomic write";
		break;
	default:
		type_str = "unknown type";
	}

	printf("Thread: %d\tAction: %s\tMO: %d\tLoc: %#014zx\tValue: %d\n", id_to_int(tid), type_str, order, (size_t)location, value);
}
