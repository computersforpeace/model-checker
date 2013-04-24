/** @file action.h
 *  @brief Models actions taken by threads.
 */

#ifndef __ACTION_H__
#define __ACTION_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "memoryorder.h"
#include "modeltypes.h"

/* Forward declarations */
class ClockVector;
class Thread;
class Promise;

namespace std {
	class mutex;
}

using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;

/**
 * @brief A recognizable don't-care value for use in the ModelAction::value
 * field
 *
 * Note that this value can be legitimately used by a program, and hence by
 * iteself does not indicate no value.
 */
#define VALUE_NONE 0xdeadbeef

/**
 * @brief The "location" at which a fence occurs
 *
 * We need a non-zero memory location to associate with fences, since our hash
 * tables don't handle NULL-pointer keys. HACK: Hopefully this doesn't collide
 * with any legitimate memory locations.
 */
#define FENCE_LOCATION ((void *)0x7)

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
	ATOMIC_UNINIT,        /**< Represents an uninitialized atomic */
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
 * @brief Represents a single atomic action
 *
 * A ModelAction is always allocated as non-snapshotting, because it is used in
 * multiple executions during backtracking. Except for fake uninitialized
 * (ATOMIC_UNINIT) ModelActions, each action is assigned a unique sequence
 * number.
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
	uint64_t get_reads_from_value() const;
	uint64_t get_write_value() const;
	uint64_t get_return_value() const;
	const ModelAction * get_reads_from() const { return reads_from; }
	Promise * get_reads_from_promise() const { return reads_from_promise; }
	std::mutex * get_mutex() const;

	Node * get_node() const;
	void set_node(Node *n) { node = n; }

	void set_read_from(const ModelAction *act);
	void set_read_from_promise(Promise *promise);

	/** Store the most recent fence-release from the same thread
	 *  @param fence The fence-release that occured prior to this */
	void set_last_fence_release(const ModelAction *fence) { last_fence_release = fence; }
	/** @return The most recent fence-release from the same thread */
	const ModelAction * get_last_fence_release() const { return last_fence_release; }

	void copy_from_new(ModelAction *newaction);
	void set_seq_number(modelclock_t num);
	void set_try_lock(bool obtainedlock);
	bool is_thread_start() const;
	bool is_thread_join() const;
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
	bool is_atomic_var() const;
	bool is_uninitialized() const;
	bool is_read() const;
	bool is_write() const;
	bool is_yield() const;
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

	Thread * get_thread_operand() const;

	void create_cv(const ModelAction *parent = NULL);
	ClockVector * get_cv() const { return cv; }
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

	bool equals(const ModelAction *x) const { return this == x; }
	bool equals(const Promise *x) const { return false; }

	bool may_read_from(const ModelAction *write) const;
	bool may_read_from(const Promise *promise) const;
	MEMALLOC
private:

	const char * get_type_str() const;
	const char * get_mo_str() const;

	/** @brief Type of action (read, write, RMW, fence, thread create, etc.) */
	action_type type;

	/** @brief The memory order for this operation. */
	memory_order order;

	/** @brief A pointer to the memory location for this action. */
	void *location;

	/** @brief The thread id that performed this action. */
	thread_id_t tid;

	/** @brief The value written (for write or RMW; undefined for read) */
	uint64_t value;

	/**
	 * @brief The store that this action reads from
	 *
	 * Only valid for reads
	 */
	const ModelAction *reads_from;

	/**
	 * @brief The promise that this action reads from
	 *
	 * Only valid for reads
	 */
	Promise *reads_from_promise;

	/** @brief The last fence release from the same thread */
	const ModelAction *last_fence_release;

	/**
	 * @brief A back reference to a Node in NodeStack
	 *
	 * Only set if this ModelAction is saved on the NodeStack. (A
	 * ModelAction can be thrown away before it ever enters the NodeStack.)
	 */
	Node *node;

	/**
	 * @brief The sequence number of this action
	 *
	 * Except for ATOMIC_UNINIT actions, this number should be unique and
	 * should represent the action's position in the execution order.
	 */
	modelclock_t seq_number;

	/**
	 * @brief The clock vector for this operation
	 *
	 * Technically, this is only needed for potentially synchronizing
	 * (e.g., non-relaxed) operations, but it is very handy to have these
	 * vectors for all operations.
	 */
	ClockVector *cv;

	bool sleep_flag;
};

#endif /* __ACTION_H__ */
