#ifndef __ACTION_H__
#define __ACTION_H__

#include <list>

#include "threads.h"
#include "libatomic.h"

#define VALUE_NONE -1

typedef enum action_type {
	THREAD_CREATE,
	THREAD_YIELD,
	THREAD_JOIN,
	ATOMIC_READ,
	ATOMIC_WRITE
} action_type_t;

/* Forward declaration */
class TreeNode;
class Node;

class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, int value);
	void print(void);

	thread_id_t get_tid() { return tid; }
	action_type get_type() { return type; }
	memory_order get_mo() { return order; }
	void * get_location() { return location; }
	int get_seq_number() const { return seq_number; }

	TreeNode * get_treenode() { return treenode; }
	void set_node(TreeNode *n) { treenode = n; }
	void set_node(Node *n) { node = n; }

	bool is_read();
	bool is_write();
	bool is_acquire();
	bool is_release();
	bool same_var(ModelAction *act);
	bool same_thread(ModelAction *act);
	bool is_dependent(ModelAction *act);

	inline bool operator <(const ModelAction& act) const {
		return get_seq_number() < act.get_seq_number();
	}
	inline bool operator >(const ModelAction& act) const {
		return get_seq_number() > act.get_seq_number();
	}
private:
	action_type type;
	memory_order order;
	void *location;
	thread_id_t tid;
	int value;
	TreeNode *treenode;
	Node *node;
	int seq_number;
};

typedef std::list<class ModelAction *> action_list_t;

#endif /* __ACTION_H__ */
