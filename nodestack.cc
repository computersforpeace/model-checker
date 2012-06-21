#include "nodestack.h"
#include "action.h"
#include "common.h"
#include "model.h"

/** @brief Node constructor */
Node::Node(ModelAction *act, int nthreads)
	: action(act),
	num_threads(nthreads),
	explored_children(num_threads),
	backtrack(num_threads),
	numBacktracks(0),
	may_read_from()
{
}

/** @brief Node desctructor */
Node::~Node()
{
	if (action)
		delete action;
}

/** Prints debugging info for the ModelAction associated with this Node */
void Node::print()
{
	if (action)
		action->print();
	else
		printf("******** empty action ********\n");
}

/**
 * Checks if the Thread associated with this thread ID has been explored from
 * this Node already.
 * @param tid is the thread ID to check
 * @return true if this thread choice has been explored already, false
 * otherwise
 */
bool Node::has_been_explored(thread_id_t tid)
{
	int id = id_to_int(tid);
	return explored_children[id];
}

/**
 * Checks if the backtracking set is empty.
 * @return true if the backtracking set is empty
 */
bool Node::backtrack_empty()
{
	return numBacktracks == 0;
}

/**
 * Explore a child Node using a given ModelAction. This updates both the
 * Node-internal and the ModelAction data to associate the ModelAction with
 * this Node.
 * @param act is the ModelAction to explore
 */
void Node::explore_child(ModelAction *act)
{
	act->set_node(this);
	explore(act->get_tid());
}

/**
 * Records a backtracking reference for a thread choice within this Node.
 * Provides feedback as to whether this thread choice is already set for
 * backtracking.
 * @return false if the thread was already set to be backtracked, true
 * otherwise
 */
bool Node::set_backtrack(thread_id_t id)
{
	int i = id_to_int(id);
	if (backtrack[i])
		return false;
	backtrack[i] = true;
	numBacktracks++;
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
	numBacktracks--;
	return int_to_id(i);
}

bool Node::is_enabled(Thread *t)
{
	return id_to_int(t->get_id()) < num_threads;
}

/**
 * Add an action to the may_read_from set.
 * @param act is the action to add
 */
void Node::add_read_from(ModelAction *act)
{
	may_read_from.insert(act);
}

void Node::explore(thread_id_t tid)
{
	int i = id_to_int(tid);
	if (backtrack[i]) {
		backtrack[i] = false;
		numBacktracks--;
	}
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
	: total_nodes(0)
{
	node_list.push_back(new Node());
	total_nodes++;
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
		iter++;
		return (*iter)->get_action();
	}

	/* Diverging from previous execution; clear out remainder of list */
	node_list_t::iterator it = iter;
	it++;
	clear_node_list(&node_list, it, node_list.end());

	/* Record action */
	get_head()->explore_child(act);
	node_list.push_back(new Node(act, model->get_num_threads()));
	total_nodes++;
	iter++;
	return NULL;
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
