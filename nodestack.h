#ifndef __NODESTACK_H__
#define __NODESTACK_H__

#include <list>
#include <vector>
#include <cstddef>
#include "threads.h"

class ModelAction;

class Node {
public:
	Node(ModelAction *act = NULL, Node *parent = NULL);
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

	void print();

	static int get_total_nodes() { return total_nodes; }
private:
	void explore(thread_id_t tid);

	static int total_nodes;
	ModelAction *action;
	int num_threads;
	std::vector<bool> explored_children;
	std::vector<bool> backtrack;
};

typedef std::list<class Node *> node_list_t;

class NodeStack {
public:
	NodeStack();
	~NodeStack();
	ModelAction * explore_action(ModelAction *act);
	Node * get_head();
	Node * get_next();
	void reset_execution();

	void print();
private:
	node_list_t node_list;
	node_list_t::iterator iter;
};

#endif /* __NODESTACK_H__ */
