/** @file action.h
 *  @brief Models actions taken by threads.
 */

#ifndef __ACTION_H__
#define __ACTION_H__

#include <list>
#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "memoryorder.h"
#include "modeltypes.h"

class ClockVector;
class Thread;

using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

/** Note that this value can be legitimately used by a program, and
		hence by iteself does not indicate no value. */

#define VALUE_NONE 0xdeadbeef

/** A special value to represent a successful trylock */

#define VALUE_TRYSUCCESS 1

/** A special value to represent a failed trylock */
#define VALUE_TRYFAILED 0

/** @brief Represents an action type, identifying one of several types of
 * ModelAction */
typedef enum action_type {
	MODEL_FIXUP_RELSEQ,   /**< Special ModelAction: finalize a release
	                       *   sequence */
	THREAD_CREATE,        /**< A thread creation action */
	THREAD_START,         /**< First action in each thread */
	THREAD_YIELD,         /**< A thread yield action */
	THREAD_JOIN,          /**< A thread join action */
	THREAD_FINISH,        /**< A thread completion action */
	ATOMIC_READ,          /**< An atomic read action */
	ATOMIC_WRITE,         /**< An atomic write action */
	ATOMIC_RMWR,          /**< The read part of an atomic RMW action */
	ATOMIC_RMW,           /**< The write part of an atomic RMW action */
	ATOMIC_RMWC,          /**< Convert an atomic RMW action into a READ */
	ATOMIC_INIT,          /**< Initialization of an atomic object (e.g.,
	                       *   atomic_init()) */
	ATOMIC_FENCE,         /**< A fence action */
	ATOMIC_LOCK,          /**< A lock action */
	ATOMIC_TRYLOCK,       /**< A trylock action */
	ATOMIC_UNLOCK,        /**< An unlock action */
	ATOMIC_NOTIFY_ONE,    /**< A notify_one action */
	ATOMIC_NOTIFY_ALL,    /**< A notify all action */
	ATOMIC_WAIT           /**< A wait action */
} action_type_t;

/* Forward declaration */
class Node;
class ClockVector;

/**
 * The ModelAction class encapsulates an atomic action.
 */
class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, uint64_t value = VALUE_NONE, Thread *thread = NULL);
	~ModelAction();
	void print() const;

	thread_id_t get_tid() const { return tid; }
	action_type get_type() const { return type; }
	memory_order get_mo() const { return order; }
	void * get_location() const { return location; }
	modelclock_t get_seq_number() const { return seq_number; }
	uint64_t get_value() const { return value; }
	const ModelAction * get_reads_from() const { return reads_from; }

	Node * get_node() const { return node; }
	void set_node(Node *n) { node = n; }

	/** Store the most recent fence-release from the same thread
	 *  @param fence The fence-release that occured prior to this */
	void set_last_fence_release(const ModelAction *fence) { last_fence_release = fence; }
	/** @return The most recent fence-release from the same thread */
	const ModelAction * get_last_fence_release() const { return last_fence_release; }

	void copy_from_new(ModelAction *newaction);
	void set_seq_number(modelclock_t num);
	void set_try_lock(bool obtainedlock);
	bool is_relseq_fixup() const;
	bool is_mutex_op() const;
	bool is_lock() const;
	bool is_trylock() const;
	bool is_unlock() const;
	bool is_wait() const;
	bool is_notify() const;
	bool is_notify_one() const;
	bool is_success_lock() const;
	bool is_failed_trylock() const;
	bool is_read() const;
	bool is_write() const;
	bool could_be_write() const;
	bool is_rmwr() const;
	bool is_rmwc() const;
	bool is_rmw() const;
	bool is_fence() const;
	bool is_initialization() const;
	bool is_relaxed() const;
	bool is_acquire() const;
	bool is_release() const;
	bool is_seqcst() const;
	bool same_var(const ModelAction *act) const;
	bool same_thread(const ModelAction *act) const;
	bool is_conflicting_lock(const ModelAction *act) const;
	bool could_synchronize_with(const ModelAction *act) const;

	void create_cv(const ModelAction *parent = NULL);
	ClockVector * get_cv() const { return cv; }
	bool read_from(const ModelAction *act);
	bool synchronize_with(const ModelAction *act);

	bool has_synchronized_with(const ModelAction *act) const;
	bool happens_before(const ModelAction *act) const;

	inline bool operator <(const ModelAction& act) const {
		return get_seq_number() < act.get_seq_number();
	}
	inline bool operator >(const ModelAction& act) const {
		return get_seq_number() > act.get_seq_number();
	}

	void process_rmw(ModelAction * act);
	void copy_typeandorder(ModelAction * act);

	void set_sleep_flag() { sleep_flag=true; }
	bool get_sleep_flag() { return sleep_flag; }
	unsigned int hash() const;

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

	/** The last fence release from the same thread */
	const ModelAction *last_fence_release;

	/** A back reference to a Node in NodeStack, if this ModelAction is
	 * saved on the NodeStack. */
	Node *node;

	modelclock_t seq_number;

	/** The clock vector stored with this action; only needed if this
	 * action is a store release? */
	ClockVector *cv;

	bool sleep_flag;
};

typedef std::list< ModelAction *, SnapshotAlloc<ModelAction *> > action_list_t;

#endif /* __ACTION_H__ */
