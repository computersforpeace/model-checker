#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <string.h>

#include "nodestack.h"
#include "action.h"
#include "common.h"
#include "model.h"
#include "threads-model.h"
#include "modeltypes.h"
#include "execution.h"

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
Node::Node(ModelAction *act, Node *par, int nthreads, Node *prevfairness) :
	read_from_status(READ_FROM_PAST),
	action(act),
	uninit_action(NULL),
	parent(par),
	num_threads(nthreads),
	explored_children(num_threads),
	backtrack(num_threads),
	fairness(num_threads),
	numBacktracks(0),
	enabled_array(NULL),
	read_from_past(),
	read_from_past_idx(0),
	read_from_promises(),
	read_from_promise_idx(-1),
	future_values(),
	future_index(-1),
	resolve_promise(),
	resolve_promise_idx(-1),
	relseq_break_writes(),
	relseq_break_index(0),
	misc_index(0),
	misc_max(0),
	yield_data(NULL)
{
	ASSERT(act);
	act->set_node(this);
	int currtid = id_to_int(act->get_tid());
	int prevtid = prevfairness ? id_to_int(prevfairness->action->get_tid()) : 0;

	if (model->params.fairwindow != 0) {
		for (int i = 0; i < num_threads; i++) {
			ASSERT(i < ((int)fairness.size()));
			struct fairness_info *fi = &fairness[i];
			struct fairness_info *prevfi = (parent && i < parent->get_num_threads()) ? &parent->fairness[i] : NULL;
			if (prevfi) {
				*fi = *prevfi;
			}
			if (parent && parent->is_enabled(int_to_id(i))) {
				fi->enabled_count++;
			}
			if (i == currtid) {
				fi->turns++;
				fi->priority = false;
			}
			/* Do window processing */
			if (prevfairness != NULL) {
				if (prevfairness->parent->is_enabled(int_to_id(i)))
					fi->enabled_count--;
				if (i == prevtid) {
					fi->turns--;
				}
				/* Need full window to start evaluating
				 * conditions
				 * If we meet the enabled count and have no
				 * turns, give us priority */
				if ((fi->enabled_count >= model->params.enabledcount) &&
						(fi->turns == 0))
					fi->priority = true;
			}
		}
	}
}

int Node::get_yield_data(int tid1, int tid2) const {
	if (tid1<num_threads && tid2 < num_threads)
		return yield_data[YIELD_INDEX(tid1,tid2,num_threads)];
	else
		return YIELD_S | YIELD_D;
}

void Node::update_yield(Scheduler * scheduler) {
	if (yield_data==NULL)
		yield_data=(int *) model_calloc(1, sizeof(int)*num_threads*num_threads);
	//handle base case
	if (parent == NULL) {
		for(int i = 0; i < num_threads*num_threads; i++) {
			yield_data[i] = YIELD_S | YIELD_D;
		}
		return;
	}
	int curr_tid=id_to_int(action->get_tid());

	for(int u = 0; u < num_threads; u++) {
		for(int v = 0; v < num_threads; v++) {
			int yield_state=parent->get_yield_data(u, v);
			bool next_enabled=scheduler->is_enabled(int_to_id(v));
			bool curr_enabled=parent->is_enabled(int_to_id(v));
			if (!next_enabled) {
				//Compute intersection of ES and E
				yield_state&=~YIELD_E;
				//Check to see if we disabled the thread
				if (u==curr_tid && curr_enabled)
					yield_state|=YIELD_D;
			}
			yield_data[YIELD_INDEX(u, v, num_threads)]=yield_state;
		}
		yield_data[YIELD_INDEX(u, curr_tid, num_threads)]=(yield_data[YIELD_INDEX(u, curr_tid, num_threads)]&~YIELD_P)|YIELD_S;
	}
	//handle curr.yield(t) part of computation
	if (action->is_yield()) {
		for(int v = 0; v < num_threads; v++) {
			int yield_state=yield_data[YIELD_INDEX(curr_tid, v, num_threads)];
			if ((yield_state & (YIELD_E | YIELD_D)) && (!(yield_state & YIELD_S)))
				yield_state |= YIELD_P;
			yield_state &= YIELD_P;
			if (scheduler->is_enabled(int_to_id(v))) {
				yield_state|=YIELD_E;
			}
			yield_data[YIELD_INDEX(curr_tid, v, num_threads)]=yield_state;
		}
	}
}

/** @brief Node desctructor */
Node::~Node()
{
	delete action;
	if (uninit_action)
		delete uninit_action;
	if (enabled_array)
		model_free(enabled_array);
	if (yield_data)
		model_free(yield_data);
}

/** Prints debugging info for the ModelAction associated with this Node */
void Node::print() const
{
	action->print();
	model_print("          thread status: ");
	if (enabled_array) {
		for (int i = 0; i < num_threads; i++) {
			char str[20];
			enabled_type_to_string(enabled_array[i], str);
			model_print("[%d: %s]", i, str);
		}
		model_print("\n");
	} else
		model_print("(info not available)\n");
	model_print("          backtrack: %s", backtrack_empty() ? "empty" : "non-empty ");
	for (int i = 0; i < (int)backtrack.size(); i++)
		if (backtrack[i] == true)
			model_print("[%d]", i);
	model_print("\n");

	model_print("          read from past: %s", read_from_past_empty() ? "empty" : "non-empty ");
	for (int i = read_from_past_idx + 1; i < (int)read_from_past.size(); i++)
		model_print("[%d]", read_from_past[i]->get_seq_number());
	model_print("\n");

	model_print("          read-from promises: %s", read_from_promise_empty() ? "empty" : "non-empty ");
	for (int i = read_from_promise_idx + 1; i < (int)read_from_promises.size(); i++)
		model_print("[%d]", read_from_promises[i]->get_seq_number());
	model_print("\n");

	model_print("          future values: %s", future_value_empty() ? "empty" : "non-empty ");
	for (int i = future_index + 1; i < (int)future_values.size(); i++)
		model_print("[%#" PRIx64 "]", future_values[i].value);
	model_print("\n");

	model_print("          promises: %s\n", promise_empty() ? "empty" : "non-empty");
	model_print("          misc: %s\n", misc_empty() ? "empty" : "non-empty");
	model_print("          rel seq break: %s\n", relseq_break_empty() ? "empty" : "non-empty");
}

/****************************** threads backtracking **************************/

/**
 * Checks if the Thread associated with this thread ID has been explored from
 * this Node already.
 * @param tid is the thread ID to check
 * @return true if this thread choice has been explored already, false
 * otherwise
 */
bool Node::has_been_explored(thread_id_t tid) const
{
	int id = id_to_int(tid);
	return explored_children[id];
}

/**
 * Checks if the backtracking set is empty.
 * @return true if the backtracking set is empty
 */
bool Node::backtrack_empty() const
{
	return (numBacktracks == 0);
}

void Node::explore(thread_id_t tid)
{
	int i = id_to_int(tid);
	ASSERT(i < ((int)backtrack.size()));
	if (backtrack[i]) {
		backtrack[i] = false;
		numBacktracks--;
	}
	explored_children[i] = true;
}

/**
 * Mark the appropriate backtracking information for exploring a thread choice.
 * @param act The ModelAction to explore
 */
void Node::explore_child(ModelAction *act, enabled_type_t *is_enabled)
{
	if (!enabled_array)
		enabled_array = (enabled_type_t *)model_malloc(sizeof(enabled_type_t) * num_threads);
	if (is_enabled != NULL)
		memcpy(enabled_array, is_enabled, sizeof(enabled_type_t) * num_threads);
	else {
		for (int i = 0; i < num_threads; i++)
			enabled_array[i] = THREAD_DISABLED;
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
	ASSERT(i < ((int)backtrack.size()));
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

void Node::clear_backtracking()
{
	for (unsigned int i = 0; i < backtrack.size(); i++)
		backtrack[i] = false;
	for (unsigned int i = 0; i < explored_children.size(); i++)
		explored_children[i] = false;
	numBacktracks = 0;
}

/************************** end threads backtracking **************************/

/*********************************** promise **********************************/

/**
 * Sets a promise to explore meeting with the given node.
 * @param i is the promise index.
 */
void Node::set_promise(unsigned int i)
{
	if (i >= resolve_promise.size())
		resolve_promise.resize(i + 1, false);
	resolve_promise[i] = true;
}

/**
 * Looks up whether a given promise should be satisfied by this node.
 * @param i The promise index.
 * @return true if the promise should be satisfied by the given ModelAction.
 */
bool Node::get_promise(unsigned int i) const
{
	return (i < resolve_promise.size()) && (int)i == resolve_promise_idx;
}

/**
 * Increments to the next promise to resolve.
 * @return true if we have a valid combination.
 */
bool Node::increment_promise()
{
	DBG();
	if (resolve_promise.empty())
		return false;
	int prev_idx = resolve_promise_idx;
	resolve_promise_idx++;
	for ( ; resolve_promise_idx < (int)resolve_promise.size(); resolve_promise_idx++)
		if (resolve_promise[resolve_promise_idx])
			return true;
	resolve_promise_idx = prev_idx;
	return false;
}

/**
 * Returns whether the promise set is empty.
 * @return true if we have explored all promise combinations.
 */
bool Node::promise_empty() const
{
	for (int i = resolve_promise_idx + 1; i < (int)resolve_promise.size(); i++)
		if (i >= 0 && resolve_promise[i])
			return false;
	return true;
}

/** @brief Clear any promise-resolution information for this Node */
void Node::clear_promise_resolutions()
{
	resolve_promise.clear();
	resolve_promise_idx = -1;
}

/******************************* end promise **********************************/

void Node::set_misc_max(int i)
{
	misc_max = i;
}

int Node::get_misc() const
{
	return misc_index;
}

bool Node::increment_misc()
{
	return (misc_index < misc_max) && ((++misc_index) < misc_max);
}

bool Node::misc_empty() const
{
	return (misc_index + 1) >= misc_max;
}

bool Node::is_enabled(Thread *t) const
{
	int thread_id = id_to_int(t->get_id());
	return thread_id < num_threads && (enabled_array[thread_id] != THREAD_DISABLED);
}

enabled_type_t Node::enabled_status(thread_id_t tid) const
{
	int thread_id = id_to_int(tid);
	if (thread_id < num_threads)
		return enabled_array[thread_id];
	else
		return THREAD_DISABLED;
}

bool Node::is_enabled(thread_id_t tid) const
{
	int thread_id = id_to_int(tid);
	return thread_id < num_threads && (enabled_array[thread_id] != THREAD_DISABLED);
}

bool Node::has_priority(thread_id_t tid) const
{
	return fairness[id_to_int(tid)].priority;
}

bool Node::has_priority_over(thread_id_t tid1, thread_id_t tid2) const
{
	return get_yield_data(id_to_int(tid1), id_to_int(tid2)) & YIELD_P;
}

/*********************************** read from ********************************/

/**
 * Get the current state of the may-read-from set iteration
 * @return The read-from type we should currently be checking (past or future)
 */
read_from_type_t Node::get_read_from_status()
{
	if (read_from_status == READ_FROM_PAST && read_from_past.empty())
		increment_read_from();
	return read_from_status;
}

/**
 * Iterate one step in the may-read-from iteration. This includes a step in
 * reading from the either the past or the future.
 * @return True if there is a new read-from to explore; false otherwise
 */
bool Node::increment_read_from()
{
	clear_promise_resolutions();
	if (increment_read_from_past()) {
	       read_from_status = READ_FROM_PAST;
	       return true;
	} else if (increment_read_from_promise()) {
		read_from_status = READ_FROM_PROMISE;
		return true;
	} else if (increment_future_value()) {
		read_from_status = READ_FROM_FUTURE;
		return true;
	}
	read_from_status = READ_FROM_NONE;
	return false;
}

/**
 * @return True if there are any new read-froms to explore
 */
bool Node::read_from_empty() const
{
	return read_from_past_empty() &&
		read_from_promise_empty() &&
		future_value_empty();
}

/**
 * Get the total size of the may-read-from set, including both past and future
 * values
 * @return The size of may-read-from
 */
unsigned int Node::read_from_size() const
{
	return read_from_past.size() +
		read_from_promises.size() +
		future_values.size();
}

/******************************* end read from ********************************/

/****************************** read from past ********************************/

/** @brief Prints info about read_from_past set */
void Node::print_read_from_past()
{
	for (unsigned int i = 0; i < read_from_past.size(); i++)
		read_from_past[i]->print();
}

/**
 * Add an action to the read_from_past set.
 * @param act is the action to add
 */
void Node::add_read_from_past(const ModelAction *act)
{
	read_from_past.push_back(act);
}

/**
 * Gets the next 'read_from_past' action from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @return The first element in read_from_past
 */
const ModelAction * Node::get_read_from_past() const
{
	if (read_from_past_idx < read_from_past.size())
		return read_from_past[read_from_past_idx];
	else
		return NULL;
}

const ModelAction * Node::get_read_from_past(int i) const
{
	return read_from_past[i];
}

int Node::get_read_from_past_size() const
{
	return read_from_past.size();
}

/**
 * Checks whether the readsfrom set for this node is empty.
 * @return true if the readsfrom set is empty.
 */
bool Node::read_from_past_empty() const
{
	return ((read_from_past_idx + 1) >= read_from_past.size());
}

/**
 * Increments the index into the readsfrom set to explore the next item.
 * @return Returns false if we have explored all items.
 */
bool Node::increment_read_from_past()
{
	DBG();
	if (read_from_past_idx < read_from_past.size()) {
		read_from_past_idx++;
		return read_from_past_idx < read_from_past.size();
	}
	return false;
}

/************************** end read from past ********************************/

/***************************** read_from_promises *****************************/

/**
 * Add an action to the read_from_promises set.
 * @param reader The read which generated the Promise; we use the ModelAction
 * instead of the Promise because the Promise does not last across executions
 */
void Node::add_read_from_promise(const ModelAction *reader)
{
	read_from_promises.push_back(reader);
}

/**
 * Gets the next 'read-from-promise' from this Node. Only valid for a node
 * where this->action is a 'read'.
 * @return The current element in read_from_promises
 */
Promise * Node::get_read_from_promise() const
{
	ASSERT(read_from_promise_idx >= 0 && read_from_promise_idx < ((int)read_from_promises.size()));
	return read_from_promises[read_from_promise_idx]->get_reads_from_promise();
}

/**
 * Gets a particular 'read-from-promise' form this Node. Only vlaid for a node
 * where this->action is a 'read'.
 * @param i The index of the Promise to get
 * @return The Promise at index i, if the Promise is still available; NULL
 * otherwise
 */
Promise * Node::get_read_from_promise(int i) const
{
	return read_from_promises[i]->get_reads_from_promise();
}

/** @return The size of the read-from-promise set */
int Node::get_read_from_promise_size() const
{
	return read_from_promises.size();
}

/**
 * Checks whether the read_from_promises set for this node is empty.
 * @return true if the read_from_promises set is empty.
 */
bool Node::read_from_promise_empty() const
{
	return ((read_from_promise_idx + 1) >= ((int)read_from_promises.size()));
}

/**
 * Increments the index into the read_from_promises set to explore the next item.
 * @return Returns false if we have explored all promises.
 */
bool Node::increment_read_from_promise()
{
	DBG();
	if (read_from_promise_idx < ((int)read_from_promises.size())) {
		read_from_promise_idx++;
		return (read_from_promise_idx < ((int)read_from_promises.size()));
	}
	return false;
}

/************************* end read_from_promises *****************************/

/****************************** future values *********************************/

/**
 * Adds a value from a weakly ordered future write to backtrack to. This
 * operation may "fail" if the future value has already been run (within some
 * sloppiness window of this expiration), or if the futurevalues set has
 * reached its maximum.
 * @see model_params.maxfuturevalues
 *
 * @param value is the value to backtrack to.
 * @return True if the future value was successully added; false otherwise
 */
bool Node::add_future_value(struct future_value fv)
{
	uint64_t value = fv.value;
	modelclock_t expiration = fv.expiration;
	thread_id_t tid = fv.tid;
	int idx = -1; /* Highest index where value is found */
	for (unsigned int i = 0; i < future_values.size(); i++) {
		if (future_values[i].value == value && future_values[i].tid == tid) {
			if (expiration <= future_values[i].expiration)
				return false;
			idx = i;
		}
	}
	if (idx > future_index) {
		/* Future value hasn't been explored; update expiration */
		future_values[idx].expiration = expiration;
		return true;
	} else if (idx >= 0 && expiration <= future_values[idx].expiration + model->params.expireslop) {
		/* Future value has been explored and is within the "sloppy" window */
		return false;
	}

	/* Limit the size of the future-values set */
	if (model->params.maxfuturevalues > 0 &&
			(int)future_values.size() >= model->params.maxfuturevalues)
		return false;

	future_values.push_back(fv);
	return true;
}

/**
 * Gets the next 'future_value' from this Node. Only valid for a node where
 * this->action is a 'read'.
 * @return The first element in future_values
 */
struct future_value Node::get_future_value() const
{
	ASSERT(future_index >= 0 && future_index < ((int)future_values.size()));
	return future_values[future_index];
}

/**
 * Checks whether the future_values set for this node is empty.
 * @return true if the future_values set is empty.
 */
bool Node::future_value_empty() const
{
	return ((future_index + 1) >= ((int)future_values.size()));
}

/**
 * Increments the index into the future_values set to explore the next item.
 * @return Returns false if we have explored all values.
 */
bool Node::increment_future_value()
{
	DBG();
	if (future_index < ((int)future_values.size())) {
		future_index++;
		return (future_index < ((int)future_values.size()));
	}
	return false;
}

/************************** end future values *********************************/

/*********************** breaking release sequences ***************************/

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
const ModelAction * Node::get_relseq_break() const
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
bool Node::relseq_break_empty() const
{
	return ((relseq_break_index + 1) >= ((int)relseq_break_writes.size()));
}

/******************* end breaking release sequences ***************************/

/**
 * Increments some behavior's index, if a new behavior is available
 * @return True if there is a new behavior available; otherwise false
 */
bool Node::increment_behaviors()
{
	/* satisfy a different misc_index values */
	if (increment_misc())
		return true;
	/* satisfy a different set of promises */
	if (increment_promise())
		return true;
	/* read from a different value */
	if (increment_read_from())
		return true;
	/* resolve a release sequence differently */
	if (increment_relseq_break())
		return true;
	return false;
}

NodeStack::NodeStack() :
	node_list(),
	head_idx(-1),
	total_nodes(0)
{
	total_nodes++;
}

NodeStack::~NodeStack()
{
	for (unsigned int i = 0; i < node_list.size(); i++)
		delete node_list[i];
}

/**
 * @brief Register the model-checker object with this NodeStack
 * @param exec The execution structure for the ModelChecker
 */
void NodeStack::register_engine(const ModelExecution *exec)
{
	this->execution = exec;
}

void NodeStack::print() const
{
	model_print("............................................\n");
	model_print("NodeStack printing node_list:\n");
	for (unsigned int it = 0; it < node_list.size(); it++) {
		if ((int)it == this->head_idx)
			model_print("vvv following action is the current iterator vvv\n");
		node_list[it]->print();
	}
	model_print("............................................\n");
}

/** Note: The is_enabled set contains what actions were enabled when
 *  act was chosen. */
ModelAction * NodeStack::explore_action(ModelAction *act, enabled_type_t *is_enabled)
{
	DBG();

	if ((head_idx + 1) < (int)node_list.size()) {
		head_idx++;
		return node_list[head_idx]->get_action();
	}

	/* Record action */
	Node *head = get_head();
	Node *prevfairness = NULL;
	if (head) {
		head->explore_child(act, is_enabled);
		if (model->params.fairwindow != 0 && head_idx > (int)model->params.fairwindow)
			prevfairness = node_list[head_idx - model->params.fairwindow];
	}

	int next_threads = execution->get_num_threads();
	if (act->get_type() == THREAD_CREATE)
		next_threads++;
	node_list.push_back(new Node(act, head, next_threads, prevfairness));
	total_nodes++;
	head_idx++;
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
	unsigned int it = head_idx + numAhead;
	for (unsigned int i = it; i < node_list.size(); i++)
		delete node_list[i];
	node_list.resize(it);
	node_list.back()->clear_backtracking();
}

Node * NodeStack::get_head() const
{
	if (node_list.empty() || head_idx < 0)
		return NULL;
	return node_list[head_idx];
}

Node * NodeStack::get_next() const
{
	if (node_list.empty()) {
		DEBUG("Empty\n");
		return NULL;
	}
	unsigned int it = head_idx + 1;
	if (it == node_list.size()) {
		DEBUG("At end\n");
		return NULL;
	}
	return node_list[it];
}

void NodeStack::reset_execution()
{
	head_idx = -1;
}
