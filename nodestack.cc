#include "nodestack.h"
#include "action.h"
#include "common.h"
#include "model.h"

/**
 * @brief Node constructor
 *
 * Constructs a single Node for use in a NodeStack. Each Node is associated
 * with exactly one ModelAction (exception: the first Node should be created
 * as an empty stub, to represent the first thread "choice") and up to one
 * parent.
 *
 * @param act The ModelAction to associate with this Node. May be NULL.
 * @param par The parent Node in the NodeStack. May be NULL if there is no
 * parent.
 * @param nthreads The number of threads which exist at this point in the
 * execution trace.
 */
Node::Node(ModelAction *act, Node *par, int nthreads)
	: action(act),
	parent(par),
	num_threads(nthreads),
	explored_children(num_threads),
	backtrack(num_threads),
	numBacktracks(0),
	may_read_from()
{
	if (act)
		act->set_node(this);
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

/** @brief Prints info about may_read_from set */
void Node::print_may_read_from()
{
	readfrom_set_t::iterator it;
	for (it = may_read_from.begin(); it != may_read_from.end(); it++)
		(*it)->print();
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
 * Mark the appropriate backtracking infromation for exploring a thread choice.
 * @param act The ModelAction to explore
 */
void Node::explore_child(ModelAction *act)
{
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
	/* Backtrack set was empty? */
	ASSERT(i != backtrack.size());

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
void Node::add_read_from(const ModelAction *act)
{
	may_read_from.push_back(act);
}

/**
 * Gets the next 'may_read_from' action from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @todo Perform reads_from backtracking/replay properly, so that this function
 * may remove elements from may_read_from
 * @return The first element in may_read_from
 */
const ModelAction * Node::get_next_read_from() {
	const ModelAction *act;
	ASSERT(!may_read_from.empty());
	act = may_read_from.front();
	/* TODO: perform reads_from replay properly */
	/* may_read_from.pop_front(); */
	return act;
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
	node_list_t::iterator it=iter;
	it++;

	if (it != node_list.end()) {
		iter++;
		return (*iter)->get_action();
	}

	/* Record action */
	get_head()->explore_child(act);
	node_list.push_back(new Node(act, get_head(), model->get_num_threads()));
	total_nodes++;
	iter++;
	return NULL;
}


void NodeStack::pop_restofstack()
{
	/* Diverging from previous execution; clear out remainder of list */
	node_list_t::iterator it = iter;
	it++;
	clear_node_list(&node_list, it, node_list.end());
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
