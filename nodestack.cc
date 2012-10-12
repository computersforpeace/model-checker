#include <string.h>

#include "nodestack.h"
#include "action.h"
#include "common.h"
#include "model.h"
#include "threads-model.h"

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
Node::Node(ModelAction *act, Node *par, int nthreads, Node *prevfairness)
	: action(act),
	parent(par),
	num_threads(nthreads),
	explored_children(num_threads),
	backtrack(num_threads),
	fairness(num_threads),
	numBacktracks(0),
	enabled_array(NULL),
	may_read_from(),
	read_from_index(0),
	future_values(),
	future_index(-1),
	relseq_break_writes(),
	relseq_break_index(0),
	misc_index(0),
	misc_max(0)
{
	if (act) {
		act->set_node(this);
		int currtid=id_to_int(act->get_tid());
		int prevtid=(prevfairness != NULL)?id_to_int(prevfairness->action->get_tid()):0;
		
		if ( model->params.fairwindow != 0 ) {
			for(int i=0;i<nthreads;i++) {
				ASSERT(i<((int)fairness.size()));
				struct fairness_info * fi=& fairness[i];
				struct fairness_info * prevfi=(par!=NULL)&&(i<par->get_num_threads())?&par->fairness[i]:NULL;
				if (prevfi) {
					*fi=*prevfi;
				}
				if (parent->is_enabled(int_to_id(i))) {
					fi->enabled_count++;
				}
				if (i==currtid) {
					fi->turns++;
					fi->priority = false;
				}
				//Do window processing
				if (prevfairness != NULL) {
					if (prevfairness -> parent->is_enabled(int_to_id(i)))
						fi->enabled_count--;
					if (i==prevtid) {
						fi->turns--;
					}
					//Need full window to start evaluating conditions
					//If we meet the enabled count and have no turns, give us priority
					if ((fi->enabled_count >= model->params.enabledcount) &&
							(fi->turns == 0))
						fi->priority = true;
				}
			}
		}
	}
}

/** @brief Node desctructor */
Node::~Node()
{
	if (action)
		delete action;
	if (enabled_array)
		model_free(enabled_array);
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
	for (unsigned int i = 0; i < may_read_from.size(); i++)
		may_read_from[i]->print();
}

/**
 * Sets a promise to explore meeting with the given node.
 * @param i is the promise index.
 */
void Node::set_promise(unsigned int i) {
	if (i >= promises.size())
		promises.resize(i + 1, PROMISE_IGNORE);
	if (promises[i] == PROMISE_IGNORE)
		promises[i] = PROMISE_UNFULFILLED;
}

/**
 * Looks up whether a given promise should be satisfied by this node.
 * @param i The promise index.
 * @return true if the promise should be satisfied by the given model action.
 */
bool Node::get_promise(unsigned int i) {
	return (i < promises.size()) && (promises[i] == PROMISE_FULFILLED);
}

/**
 * Increments to the next combination of promises.
 * @return true if we have a valid combination.
 */
bool Node::increment_promise() {
	DBG();

	for (unsigned int i = 0; i < promises.size(); i++) {
		if (promises[i] == PROMISE_UNFULFILLED) {
			promises[i] = PROMISE_FULFILLED;
			while (i > 0) {
				i--;
				if (promises[i] == PROMISE_FULFILLED)
					promises[i] = PROMISE_UNFULFILLED;
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
	for (unsigned int i = 0; i < promises.size();i++)
		if (promises[i] == PROMISE_UNFULFILLED)
			return false;
	return true;
}


void Node::set_misc_max(int i) {
	misc_max=i;
}

int Node::get_misc() {
	return misc_index;
}

bool Node::increment_misc() {
	return (misc_index<misc_max)&&((++misc_index)<misc_max);
}

bool Node::misc_empty() {
	return (misc_index+1)>=misc_max;
}


/**
 * Adds a value from a weakly ordered future write to backtrack to.
 * @param value is the value to backtrack to.
 */
bool Node::add_future_value(uint64_t value, modelclock_t expiration) {
	int suitableindex=-1;
	for (unsigned int i = 0; i < future_values.size(); i++) {
		if (future_values[i].value == value) {
			if (future_values[i].expiration>=expiration)
				return false;
			if (future_index < ((int) i)) {
				suitableindex=i;
			}
		}
	}

	if (suitableindex!=-1) {
		future_values[suitableindex].expiration=expiration;
		return true;
	}
	struct future_value newfv={value, expiration};
	future_values.push_back(newfv);
	return true;
}

/**
 * Checks whether the future_values set for this node is empty.
 * @return true if the future_values set is empty.
 */
bool Node::future_value_empty() {
	return ((future_index + 1) >= ((int)future_values.size()));
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
	return ((read_from_index+1) >= may_read_from.size());
}

/**
 * Mark the appropriate backtracking information for exploring a thread choice.
 * @param act The ModelAction to explore
 */
void Node::explore_child(ModelAction *act, enabled_type_t * is_enabled)
{
	if ( ! enabled_array )
		enabled_array=(enabled_type_t *)model_malloc(sizeof(enabled_type_t)*num_threads);
	if (is_enabled != NULL)
		memcpy(enabled_array, is_enabled, sizeof(enabled_type_t)*num_threads);
	else {
		for(int i=0;i<num_threads;i++)
			enabled_array[i]=THREAD_DISABLED;
	}

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
	int thread_id=id_to_int(t->get_id());
	return thread_id < num_threads && (enabled_array[thread_id] != THREAD_DISABLED);
}

bool Node::is_enabled(thread_id_t tid)
{
	int thread_id=id_to_int(tid);
	return thread_id < num_threads && (enabled_array[thread_id] != THREAD_DISABLED);
}

bool Node::has_priority(thread_id_t tid)
{
	return fairness[id_to_int(tid)].priority;
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
	ASSERT(future_index<((int)future_values.size()));
	return future_values[future_index].value;
}

modelclock_t Node::get_future_value_expiration() {
	ASSERT(future_index<((int)future_values.size()));
	return future_values[future_index].expiration;
}


int Node::get_read_from_size() {
	return may_read_from.size();
}

const ModelAction * Node::get_read_from_at(int i) {
	return may_read_from[i];
}

/**
 * Gets the next 'may_read_from' action from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @return The first element in may_read_from
 */
const ModelAction * Node::get_read_from() {
	if (read_from_index < may_read_from.size())
		return may_read_from[read_from_index];
	else
		return NULL;
}

/**
 * Increments the index into the readsfrom set to explore the next item.
 * @return Returns false if we have explored all items.
 */
bool Node::increment_read_from() {
	DBG();
	promises.clear();
	if (read_from_index < may_read_from.size()) {
		read_from_index++;
		return read_from_index < may_read_from.size();
	}
	return false;
}

/**
 * Increments the index into the future_values set to explore the next item.
 * @return Returns false if we have explored all values.
 */
bool Node::increment_future_value() {
	DBG();
	promises.clear();
	if (future_index < ((int)future_values.size())) {
		future_index++;
		return (future_index < ((int)future_values.size()));
	}
	return false;
}

/**
 * Add a write ModelAction to the set of writes that may break the release
 * sequence. This is used during replay exploration of pending release
 * sequences. This Node must correspond to a release sequence fixup action.
 *
 * @param write The write that may break the release sequence. NULL means we
 * allow the release sequence to synchronize.
 */
void Node::add_relseq_break(const ModelAction *write)
{
	relseq_break_writes.push_back(write);
}

/**
 * Get the write that may break the current pending release sequence,
 * according to the replay / divergence pattern.
 *
 * @return A write that may break the release sequence. If NULL, that means
 * the release sequence should not be broken.
 */
const ModelAction * Node::get_relseq_break()
{
	if (relseq_break_index < (int)relseq_break_writes.size())
		return relseq_break_writes[relseq_break_index];
	else
		return NULL;
}

/**
 * Increments the index into the relseq_break_writes set to explore the next
 * item.
 * @return Returns false if we have explored all values.
 */
bool Node::increment_relseq_break()
{
	DBG();
	promises.clear();
	if (relseq_break_index < ((int)relseq_break_writes.size())) {
		relseq_break_index++;
		return (relseq_break_index < ((int)relseq_break_writes.size()));
	}
	return false;
}

/**
 * @return True if all writes that may break the release sequence have been
 * explored
 */
bool Node::relseq_break_empty() {
	return ((relseq_break_index + 1) >= ((int)relseq_break_writes.size()));
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

NodeStack::NodeStack() :
	node_list(1, new Node()),
	iter(0),
	total_nodes(0)
{
	total_nodes++;
}

NodeStack::~NodeStack()
{
	for (unsigned int i = 0; i < node_list.size(); i++)
		delete node_list[i];
}

void NodeStack::print()
{
	printf("............................................\n");
	printf("NodeStack printing node_list:\n");
	for (unsigned int it = 0; it < node_list.size(); it++) {
		if (it == this->iter)
			printf("vvv following action is the current iterator vvv\n");
		node_list[it]->print();
	}
	printf("............................................\n");
}

/** Note: The is_enabled set contains what actions were enabled when
 *  act was chosen. */

ModelAction * NodeStack::explore_action(ModelAction *act, enabled_type_t * is_enabled)
{
	DBG();

	ASSERT(!node_list.empty());

	if ((iter+1) < node_list.size()) {
		iter++;
		return node_list[iter]->get_action();
	}

	/* Record action */
	get_head()->explore_child(act, is_enabled);
	Node *prevfairness = NULL;
	if ( model->params.fairwindow != 0 && iter > model->params.fairwindow ) {
		prevfairness = node_list[iter-model->params.fairwindow];
	}
	node_list.push_back(new Node(act, get_head(), model->get_num_threads(), prevfairness));
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
	unsigned int it=iter+numAhead;
	for(unsigned int i=it;i<node_list.size();i++)
		delete node_list[i];
	node_list.resize(it);
}

Node * NodeStack::get_head()
{
	if (node_list.empty())
		return NULL;
	return node_list[iter];
}

Node * NodeStack::get_next()
{
	if (node_list.empty()) {
		DEBUG("Empty\n");
		return NULL;
	}
	unsigned int it=iter+1;
	if (it == node_list.size()) {
		DEBUG("At end\n");
		return NULL;
	}
	return node_list[it];
}

void NodeStack::reset_execution()
{
	iter = 0;
}
