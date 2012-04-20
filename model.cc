#include <stdio.h>

#include "model.h"
#include "schedule.h"
#include "common.h"

ModelChecker *model;

ModelChecker::ModelChecker()
{
	/* First thread created (system_thread) will have id 1 */
	this->used_thread_id = 0;
	/* Initialize default scheduler */
	this->scheduler = new Scheduler();

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

void ModelChecker::assign_id(Thread *t)
{
	t->set_id(++used_thread_id);
}

void ModelChecker::add_system_thread(Thread *t)
{
	this->system_thread = t;
}

Thread *ModelChecker::schedule_next_thread()
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

ModelAction *ModelChecker::get_last_conflict(ModelAction *act)
{
	void *loc = act->get_location();
	action_type type = act->get_type();
	thread_id_t id = act->get_tid();

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
		if (prev->get_location() != loc)
			continue;
		if (type == ATOMIC_READ && prev->get_type() != ATOMIC_WRITE)
			continue;
		/* Conflict from the same thread is not really a conflict */
		if (id == prev->get_tid())
			return NULL;
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

	printf("Setting backtrack: conflict = %d, instead tid = %d\n",
			prev->get_tid(), act->get_tid());
	prev->print();
	act->print();

	Backtrack *back = new Backtrack(prev, action_trace);
	backtrack_list.push_back(back);
}

void ModelChecker::check_current_action(void)
{
	ModelAction *next = this->current_action;

	if (!next) {
		DEBUG("trying to push NULL action...\n");
		return;
	}
	nextThread = advance_backtracking_state();
	next->set_node(currentNode);
	set_backtracking(next);
	currentNode = currentNode->exploreChild(next->get_tid());
	this->action_trace->push_back(next);
}

void ModelChecker::print_trace(void)
{
	action_list_t::iterator it;

	printf("\n");
	printf("---------------------------------------------------------------------\n");
	printf("Total nodes created: %d\n\n", TreeNode::getTotalNodes());

	for (it = action_trace->begin(); it != action_trace->end(); it++) {
		DBG();
		(*it)->print();
	}
	printf("---------------------------------------------------------------------\n");
}

int ModelChecker::add_thread(Thread *t)
{
	thread_map[t->get_id()] = t;
	return 0;
}

int ModelChecker::switch_to_master(ModelAction *act)
{
	Thread *old, *next;

	DBG();
	old = thread_current();
	set_current_action(act);
	old->set_state(THREAD_READY);
	next = system_thread;
	return old->swap(next);
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

	printf("Thread: %d\tAction: %s\tMO: %d\tLoc: %#014zx\tValue: %d\n", tid, type_str, order, (size_t)location, value);
}
