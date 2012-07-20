/** @file action.h
 *  @brief Models actions taken by threads.
 */

#ifndef __ACTION_H__
#define __ACTION_H__

#include <list>
#include <cstddef>

#include "threads.h"
#include "mymemory.h"
#include "clockvector.h"
#include "memoryorder.h"

using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

/** Note that this value can be legitimately used by a program, and
		hence by iteself does not indicate no value. */

#define VALUE_NONE 1234567890

/** @brief Represents an action type, identifying one of several types of
 * ModelAction */
typedef enum action_type {
	THREAD_CREATE,        /**< A thread creation action */
	THREAD_START,         /**< First action in each thread */
	THREAD_YIELD,         /**< A thread yield action */
	THREAD_JOIN,          /**< A thread join action */
	ATOMIC_READ,          /**< An atomic read action */
	ATOMIC_WRITE,         /**< An atomic write action */
	ATOMIC_RMWR,          /**< The read of an atomic read-modify-write action */
	ATOMIC_RMW,           /**< The write of an atomic read-modify-write action */
	ATOMIC_RMWC,          /**< Terminate an atomic read-modify-write action w/o write */
	ATOMIC_INIT           /**< Initialization of an atomic object (e.g.,
	                       *   atomic_init()) */
} action_type_t;

/* Forward declaration */
class Node;
class ClockVector;

/**
 * The ModelAction class encapsulates an atomic action.
 */
class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, uint64_t value = VALUE_NONE);
	~ModelAction();
	void print(void) const;

	thread_id_t get_tid() const { return tid; }
	action_type get_type() const { return type; }
	memory_order get_mo() const { return order; }
	void * get_location() const { return location; }
	modelclock_t get_seq_number() const { return seq_number; }
	uint64_t get_value() const { return value; }
	const ModelAction * get_reads_from() const { return reads_from; }

	Node * get_node() const { return node; }
	void set_node(Node *n) { node = n; }

	bool is_read() const;
	bool is_write() const;
	bool is_rmwr() const;
	bool is_rmwc() const;
	bool is_rmw() const;
	bool is_initialization() const;
	bool is_acquire() const;
	bool is_release() const;
	bool is_seqcst() const;
	bool same_var(const ModelAction *act) const;
	bool same_thread(const ModelAction *act) const;
	bool is_synchronizing(const ModelAction *act) const;

	void create_cv(const ModelAction *parent = NULL);
	ClockVector * get_cv() const { return cv; }
	void read_from(const ModelAction *act);

	bool happens_before(const ModelAction *act) const;

	inline bool operator <(const ModelAction& act) const {
		return get_seq_number() < act.get_seq_number();
	}
	inline bool operator >(const ModelAction& act) const {
		return get_seq_number() > act.get_seq_number();
	}

	void process_rmw(ModelAction * act);
	void copy_typeandorder(ModelAction * act);

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

	/** The value written (for write or RMW; undefined for read) */
	uint64_t value;

	/** The action that this action reads from. Only valid for reads */
	const ModelAction *reads_from;

	/** A back reference to a Node in NodeStack, if this ModelAction is
	 * saved on the NodeStack. */
	Node *node;

	modelclock_t seq_number;

	/** The clock vector stored with this action; only needed if this
	 * action is a store release? */
	ClockVector *cv;
};

typedef std::list<ModelAction *> action_list_t;

#endif /* __ACTION_H__ */
