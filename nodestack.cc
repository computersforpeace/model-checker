#include "nodestack.h"
#include "action.h"
#include "common.h"

int Node::total_nodes = 0;

Node::Node(ModelAction *act, Node *parent)
	: action(act),
	num_threads(parent ? parent->num_threads : 1),
	explored_children(num_threads),
	backtrack(num_threads)
{
	total_nodes++;
	if (act && act->get_type() == THREAD_CREATE) {
		num_threads++;
		explored_children.resize(num_threads);
		backtrack.resize(num_threads);
	}
}

Node::~Node()
{
	if (action)
		delete action;
}

void Node::print()
{
	if (action)
		action->print();
	else
		printf("******** empty action ********\n");
}

bool Node::has_been_explored(thread_id_t tid)
{
	int id = id_to_int(tid);
	return explored_children[id];
}

bool Node::backtrack_empty()
{
	unsigned int i;
	for (i = 0; i < backtrack.size(); i++)
		if (backtrack[i] == true)
			return false;
	return true;
}

void Node::explore_child(ModelAction *act)
{
	act->set_node(this);
	explore(act->get_tid());
}

bool Node::set_backtrack(thread_id_t id)
{
	int i = id_to_int(id);
	if (backtrack[i])
		return false;
	backtrack[i] = true;
	return true;
}

thread_id_t Node::get_next_backtrack()
{
	/* TODO: find next backtrack */
	unsigned int i;
	for (i = 0; i < backtrack.size(); i++)
		if (backtrack[i] == true)
			break;
	if (i >= backtrack.size())
		return THREAD_ID_T_NONE;
	backtrack[i] = false;
	return int_to_id(i);
}

bool Node::is_enabled(Thread *t)
{
	return id_to_int(t->get_id()) < num_threads;
}

void Node::explore(thread_id_t tid)
{
	int i = id_to_int(tid);
	backtrack[i] = false;
	explored_children[i] = true;
}

static void clear_node_list(node_list_t *list, node_list_t::iterator start,
					       node_list_t::iterator end)
{
	node_list_t::iterator it;

	for (it = start; it != end; it++)
		delete (*it);
	list->erase(start, end);
}

NodeStack::NodeStack()
{
	node_list.push_back(new Node());
	iter = node_list.begin();
}

NodeStack::~NodeStack()
{
	clear_node_list(&node_list, node_list.begin(), node_list.end());
}

void NodeStack::print()
{
	node_list_t::iterator it;
	printf("............................................\n");
	printf("NodeStack printing node_list:\n");
	for (it = node_list.begin(); it != node_list.end(); it++) {
		if (it == this->iter)
			printf("vvv following action is the current iterator vvv\n");
		(*it)->print();
	}
	printf("............................................\n");
}

ModelAction * NodeStack::explore_action(ModelAction *act)
{
	DBG();

	ASSERT(!node_list.empty());

	if (get_head()->has_been_explored(act->get_tid())) {
		/* Discard duplicate ModelAction */
		delete act;
		iter++;
	} else { /* Diverging from previous execution */
		/* Clear out remainder of list */
		node_list_t::iterator it = iter;
		it++;
		clear_node_list(&node_list, it, node_list.end());

		/* Record action */
		get_head()->explore_child(act);
		node_list.push_back(new Node(act, get_head()));
		iter++;
	}
	return (*iter)->get_action();
}

Node * NodeStack::get_head()
{
	if (node_list.empty())
		return NULL;
	return *iter;
}

Node * NodeStack::get_next()
{
	node_list_t::iterator it = iter;
	if (node_list.empty()) {
		DEBUG("Empty\n");
		return NULL;
	}
	it++;
	if (it == node_list.end()) {
		DEBUG("At end\n");
		return NULL;
	}
	return *it;
}

void NodeStack::reset_execution()
{
	iter = node_list.begin();
}
