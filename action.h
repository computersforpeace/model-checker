/** @file action.h
 *  @brief Models actions taken by threads.
 */

#ifndef __ACTION_H__
#define __ACTION_H__

#include <list>
#include <cstddef>

#include "threads.h"
#include "libatomic.h"
#include "mymemory.h"
#define VALUE_NONE -1

typedef enum action_type {
	THREAD_CREATE,
	THREAD_YIELD,
	THREAD_JOIN,
	ATOMIC_READ,
	ATOMIC_WRITE,
	ATOMIC_RMW
} action_type_t;

/* Forward declaration */
class Node;
class ClockVector;
/**
 * The ModelAction class encapsulates an atomic action.
 */

class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, int value);
	~ModelAction();
	void print(void);

	thread_id_t get_tid() { return tid; }
	action_type get_type() { return type; }
	memory_order get_mo() { return order; }
	void * get_location() { return location; }
	int get_seq_number() const { return seq_number; }

	Node * get_node() { return node; }
	void set_node(Node *n) { node = n; }

	bool is_read();
	bool is_write();
	bool is_rmw();
	bool is_acquire();
	bool is_release();
	bool is_seqcst();
	bool same_var(ModelAction *act);
	bool same_thread(ModelAction *act);
	bool is_synchronizing(ModelAction *act);

	void create_cv(ModelAction *parent = NULL);
	void read_from(ModelAction *act);

	inline bool operator <(const ModelAction& act) const {
		return get_seq_number() < act.get_seq_number();
	}
	inline bool operator >(const ModelAction& act) const {
		return get_seq_number() > act.get_seq_number();
	}

	MEMALLOC
private:

	/** Type of action (read, write, thread create, thread yield, thread join) */
	action_type type;

	/** The memory order for this operation. */
	memory_order order;

	/** A pointer to the memory location for this action. */
	void *location;

	/** The thread id that performed this action. */
	thread_id_t tid;
	
	/** The value written.  This should probably be something longer. */
	int value;

	Node *node;
	
	int seq_number;

	/** The clock vector stored with this action if this action is a
	 *  store release */

	ClockVector *cv;
};

typedef std::list<class ModelAction *> action_list_t;

#endif /* __ACTION_H__ */
