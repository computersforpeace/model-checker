/** @file nodestack.h
 *  @brief Stack of operations for use in backtracking.
*/

#ifndef __NODESTACK_H__
#define __NODESTACK_H__

#include <list>
#include <vector>
#include <cstddef>
#include "threads.h"
#include "mymemory.h"

class ModelAction;

typedef std::vector< const ModelAction *, MyAlloc< const ModelAction * > > readfrom_set_t;
typedef std::vector< uint64_t, MyAlloc< uint64_t > > futurevalues_t;
typedef std::vector< uint32_t, MyAlloc< uint32_t > > promises_t;

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
	Node(ModelAction *act = NULL, Node *par = NULL, int nthreads = 1);
	~Node();
	/* return true = thread choice has already been explored */
	bool has_been_explored(thread_id_t tid);
	/* return true = backtrack set is empty */
	bool backtrack_empty();

	void explore_child(ModelAction *act);
	/* return false = thread was already in backtrack */
	bool set_backtrack(thread_id_t id);
	thread_id_t get_next_backtrack();
	bool is_enabled(Thread *t);
	ModelAction * get_action() { return action; }

	/** @return the parent Node to this Node; that is, the action that
	 * occurred previously in the stack. */
	Node * get_parent() const { return parent; }

	bool add_future_value(uint64_t value);
	uint64_t get_future_value();
	bool increment_future_value();
	bool future_value_empty();

	void add_read_from(const ModelAction *act);
	const ModelAction * get_read_from();
	bool increment_read_from();
	bool read_from_empty();

	void set_promise(uint32_t i);
	bool get_promise(uint32_t i);
	bool increment_promise();
	bool promise_empty();

	void print();
	void print_may_read_from();

	MEMALLOC
private:
	void explore(thread_id_t tid);

	ModelAction *action;
	Node *parent;
	int num_threads;
	std::vector< bool, MyAlloc<bool> > explored_children;
	std::vector< bool, MyAlloc<bool> > backtrack;
	int numBacktracks;

	/** The set of ModelActions that this the action at this Node may read
	 *  from. Only meaningful if this Node represents a 'read' action. */
	readfrom_set_t may_read_from;
	unsigned int read_from_index;

	futurevalues_t future_values;
	promises_t promises;
	unsigned int future_index;
};

typedef std::list< Node *, MyAlloc< Node * > > node_list_t;

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
	ModelAction * explore_action(ModelAction *act);
	Node * get_head();
	Node * get_next();
	void reset_execution();
	void pop_restofstack(int numAhead);
	int get_total_nodes() { return total_nodes; }

	void print();

	MEMALLOC
private:
	node_list_t node_list;
	node_list_t::iterator iter;

	int total_nodes;
};

#endif /* __NODESTACK_H__ */
