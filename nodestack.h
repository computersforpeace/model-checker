/** @file nodestack.h
 *  @brief Stack of operations for use in backtracking.
*/

#ifndef __NODESTACK_H__
#define __NODESTACK_H__

#include <list>
#include <vector>
#include <set>
#include <cstddef>
#include "threads.h"
#include "mymemory.h"

class ModelAction;

typedef std::set< ModelAction *, std::less< ModelAction *>, MyAlloc< ModelAction * > > action_set_t;

class Node {
public:
	Node(ModelAction *act = NULL, int nthreads = 1);
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

	void add_read_from(ModelAction *act);

	void print();

	MEMALLOC
private:
	void explore(thread_id_t tid);

	ModelAction *action;
	int num_threads;
	std::vector< bool, MyAlloc<bool> > explored_children;
	std::vector< bool, MyAlloc<bool> > backtrack;
	int numBacktracks;

	/** The set of ModelActions that this the action at this Node may read
	 *  from. Only meaningful if this Node represents a 'read' action. */
	action_set_t may_read_from;
};

typedef std::list<class Node *, MyAlloc< class Node * > > node_list_t;

class NodeStack {
public:
	NodeStack();
	~NodeStack();
	ModelAction * explore_action(ModelAction *act);
	Node * get_head();
	Node * get_next();
	void reset_execution();

	int get_total_nodes() { return total_nodes; }

	void print();

	MEMALLOC
private:
	node_list_t node_list;
	node_list_t::iterator iter;

	int total_nodes;
};

#endif /* __NODESTACK_H__ */
