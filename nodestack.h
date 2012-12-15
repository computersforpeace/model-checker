/** @file nodestack.h
 *  @brief Stack of operations for use in backtracking.
*/

#ifndef __NODESTACK_H__
#define __NODESTACK_H__

#include <list>
#include <vector>
#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "modeltypes.h"
#include "schedule.h"

class ModelAction;
class Thread;

/**
 * A flag used for the promise counting/combination problem within a node,
 * denoting whether a particular Promise is
 * <ol><li>@b applicable: can be satisfied by this Node's ModelAction and</li>
 * <li>@b fulfilled: satisfied by this Node's ModelAction under the current
 * configuration.</li></ol>
 */

#define	PROMISE_IGNORE 0 /**< This promise is inapplicable; ignore it */
#define	PROMISE_UNFULFILLED 1 /**< This promise is applicable but unfulfilled */
#define	PROMISE_FULFILLED 2 /**< This promise is applicable and fulfilled */
#define PROMISE_MASK 0xf
#define PROMISE_RMW 0x10

typedef int promise_t;

struct future_value {
	uint64_t value;
	modelclock_t expiration;
};

struct fairness_info {
	unsigned int enabled_count;
	unsigned int turns;
	bool priority;
};


/**
 * @brief A single node in a NodeStack
 *
 * Represents a single node in the NodeStack. Each Node is associated with up
 * to one action and up to one parent node. A node holds information
 * regarding the last action performed (the "associated action"), the thread
 * choices that have been explored (explored_children) and should be explored
 * (backtrack), and the actions that the last action may read from.
 */
class Node {
public:
	Node(ModelAction *act, Node *par, int nthreads, Node *prevfairness);
	~Node();
	/* return true = thread choice has already been explored */
	bool has_been_explored(thread_id_t tid) const;
	/* return true = backtrack set is empty */
	bool backtrack_empty() const;

	void explore_child(ModelAction *act, enabled_type_t *is_enabled);
	/* return false = thread was already in backtrack */
	bool set_backtrack(thread_id_t id);
	thread_id_t get_next_backtrack();
	bool is_enabled(Thread *t) const;
	bool is_enabled(thread_id_t tid) const;
	enabled_type_t enabled_status(thread_id_t tid) const;

	ModelAction * get_action() const { return action; }
	bool has_priority(thread_id_t tid) const;
	int get_num_threads() const { return num_threads; }
	/** @return the parent Node to this Node; that is, the action that
	 * occurred previously in the stack. */
	Node * get_parent() const { return parent; }

	bool add_future_value(uint64_t value, modelclock_t expiration);
	uint64_t get_future_value() const;
	modelclock_t get_future_value_expiration() const;
	bool increment_future_value();
	bool future_value_empty() const;

	void add_read_from(const ModelAction *act);
	const ModelAction * get_read_from() const;
	bool increment_read_from();
	bool read_from_empty() const;
	int get_read_from_size() const;
	const ModelAction * get_read_from_at(int i) const;

	void set_promise(unsigned int i, bool is_rmw);
	bool get_promise(unsigned int i) const;
	bool increment_promise();
	bool promise_empty() const;
	enabled_type_t *get_enabled_array() {return enabled_array;}

	void set_misc_max(int i);
	int get_misc() const;
	bool increment_misc();
	bool misc_empty() const;

	void add_relseq_break(const ModelAction *write);
	const ModelAction * get_relseq_break() const;
	bool increment_relseq_break();
	bool relseq_break_empty() const;

	void print();
	void print_may_read_from();

	MEMALLOC
private:
	void explore(thread_id_t tid);

	ModelAction * const action;
	Node * const parent;
	const int num_threads;
	std::vector< bool, ModelAlloc<bool> > explored_children;
	std::vector< bool, ModelAlloc<bool> > backtrack;
	std::vector< struct fairness_info, ModelAlloc< struct fairness_info> > fairness;
	int numBacktracks;
	enabled_type_t *enabled_array;

	/** The set of ModelActions that this the action at this Node may read
	 *  from. Only meaningful if this Node represents a 'read' action. */
	std::vector< const ModelAction *, ModelAlloc< const ModelAction * > > may_read_from;

	unsigned int read_from_index;

	std::vector< struct future_value, ModelAlloc<struct future_value> > future_values;
	std::vector< promise_t, ModelAlloc<promise_t> > promises;
	int future_index;

	std::vector< const ModelAction *, ModelAlloc<const ModelAction *> > relseq_break_writes;
	int relseq_break_index;

	int misc_index;
	int misc_max;
};

typedef std::vector< Node *, ModelAlloc< Node * > > node_list_t;

/**
 * @brief A stack of nodes
 *
 * Holds a Node linked-list that can be used for holding backtracking,
 * may-read-from, and replay information. It is used primarily as a
 * stack-like structure, in that backtracking points and replay nodes are
 * only removed from the top (most recent).
 */
class NodeStack {
public:
	NodeStack();
	~NodeStack();
	ModelAction * explore_action(ModelAction *act, enabled_type_t * is_enabled);
	Node * get_head() const;
	Node * get_next() const;
	void reset_execution();
	void pop_restofstack(int numAhead);
	int get_total_nodes() { return total_nodes; }

	void print() const;

	MEMALLOC
private:
	node_list_t node_list;

	/**
	 * @brief the index position of the current head Node
	 *
	 * This index is relative to node_list. The index should point to the
	 * current head Node. It is negative when the list is empty.
	 */
	int head_idx;

	int total_nodes;
};

#endif /* __NODESTACK_H__ */
