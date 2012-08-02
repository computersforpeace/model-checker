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
	may_read_from(),
	read_from_index(0),
	future_values(),
	future_index(-1)
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
 * Sets a promise to explore meeting with the given node.
 * @param i is the promise index.
 */
void Node::set_promise(uint32_t i) {
	if (i>=promises.size())
		promises.resize(i+1,0);
	promises[i]=1;
}

/**
 * Looks up whether a given promise should be satisfied by this node.
 * @param i The promise index.
 * @return true if the promise should be satisfied by the given model action.
 */
bool Node::get_promise(uint32_t i) {
	return (i<promises.size())&&(promises[i]==2);
}

/**
 * Increments to the next combination of promises.
 * @return true if we have a valid combination.
 */
bool Node::increment_promise() {
	for (unsigned int i=0;i<promises.size();i++) {
		if (promises[i]==1) {
			promises[i]=2;
			while (i>0) {
				i--;
				if (promises[i]==2)
					promises[i]=1;
			}
			return true;
		}
	}
	return false;
}

/**
 * Returns whether the promise set is empty.
 * @return true if we have explored all promise combinations.
 */
bool Node::promise_empty() {
	for (unsigned int i=0;i<promises.size();i++)
		if (promises[i]==1)
			return false;
	return true;
}

/**
 * Adds a value from a weakly ordered future write to backtrack to.
 * @param value is the value to backtrack to.
 */
bool Node::add_future_value(uint64_t value) {
	for(unsigned int i=0;i<future_values.size();i++)
		if (future_values[i]==value)
			return false;

	future_values.push_back(value);
	return true;
}

/**
 * Checks whether the future_values set for this node is empty.
 * @return true if the future_values set is empty.
 */
bool Node::future_value_empty() {
	return ((future_index+1)>=future_values.size());
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
	return (numBacktracks == 0);
}

/**
 * Checks whether the readsfrom set for this node is empty.
 * @return true if the readsfrom set is empty.
 */
bool Node::read_from_empty() {
	return ((read_from_index+1)>=may_read_from.size());
}

/**
 * Mark the appropriate backtracking information for exploring a thread choice.
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
	/** @todo Find next backtrack */
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
 * Gets the next 'future_value' value from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @return The first element in future_values
 */
uint64_t Node::get_future_value() {
	ASSERT(future_index<future_values.size());
	return future_values[future_index];
}

/**
 * Gets the next 'may_read_from' action from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @return The first element in may_read_from
 */
const ModelAction * Node::get_read_from() {
	if (read_from_index<may_read_from.size())
		return may_read_from[read_from_index];
	else
		return NULL;
}

/**
 * Increments the index into the readsfrom set to explore the next item.
 * @return Returns false if we have explored all items.
 */
bool Node::increment_read_from() {
	read_from_index++;
	return (read_from_index<may_read_from.size());
}

/**
 * Increments the index into the future_values set to explore the next item.
 * @return Returns false if we have explored all values.
 */
bool Node::increment_future_value() {
	future_index++;
	return (future_index<future_values.size());
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

/**
 * Empties the stack of all trailing nodes after a given position and calls the
 * destructor for each. This function is provided an offset which determines
 * how many nodes (relative to the current replay state) to save before popping
 * the stack.
 * @param numAhead gives the number of Nodes (including this Node) to skip over
 * before removing nodes.
 */
void NodeStack::pop_restofstack(int numAhead)
{
	/* Diverging from previous execution; clear out remainder of list */
	node_list_t::iterator it = iter;
	while (numAhead--)
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
