/** @file threads-model.h
 *  @brief Model Checker Thread class.
 */

#ifndef __THREADS_MODEL_H__
#define __THREADS_MODEL_H__

#include <ucontext.h>
#include <stdint.h>
#include <vector>

#include "mymemory.h"
#include <threads.h>
#include "modeltypes.h"

/** @brief Represents the state of a user Thread */
typedef enum thread_state {
	/** Thread was just created and hasn't run yet */
	THREAD_CREATED,
	/** Thread is running */
	THREAD_RUNNING,
	/** Thread is not currently running but is ready to run */
	THREAD_READY,
	/**
	 * Thread is waiting on another action (e.g., thread completion, lock
	 * release, etc.)
	 */
	THREAD_BLOCKED,
	/** Thread has completed its execution */
	THREAD_COMPLETED
} thread_state;

class ModelAction;

/** @brief A Thread is created for each user-space thread */
class Thread {
public:
	Thread(thread_id_t tid);
	Thread(thrd_t *t, void (*func)(void *), void *a);
	~Thread();
	void complete();

	static int swap(ucontext_t *ctxt, Thread *t);
	static int swap(Thread *t, ucontext_t *ctxt);

	thread_state get_state() { return state; }
	void set_state(thread_state s) { state = s; }
	thread_id_t get_id();
	thrd_t get_thrd_t() { return *user_thread; }
	Thread * get_parent() { return parent; }

	void set_creation(ModelAction *act) { creation = act; }
	ModelAction * get_creation() { return creation; }

	/**
	 * Set a return value for the last action in this thread (e.g., for an
	 * atomic read).
	 * @param value The value to return
	 */
	void set_return_value(uint64_t value) { last_action_val = value; }

	/**
	 * Retrieve a return value for the last action in this thread. Used,
	 * for instance, for an atomic read to return the 'read' value. Should
	 * be called from a user context.
	 * @return The value 'returned' by the action
	 */
	uint64_t get_return_value() { return last_action_val; }

	/** @return True if this thread is finished executing */
	bool is_complete() { return state == THREAD_COMPLETED; }

	/** @return True if this thread is blocked */
	bool is_blocked() { return state == THREAD_BLOCKED; }

	/** @return True if no threads are waiting on this Thread */
	bool wait_list_empty() { return wait_list.empty(); }

	/**
	 * Add a ModelAction to the waiting list for this thread.
	 * @param t The ModelAction to add. Must be a JOIN.
	 */
	void push_wait_list(ModelAction *act) { wait_list.push_back(act); }

	unsigned int num_wait_list() {
		return wait_list.size();
	}

	ModelAction * get_waiter(unsigned int i) {
		return wait_list[i];
	}

	ModelAction * get_pending() { return pending; }
	void set_pending(ModelAction *act) { pending = act; }
	/**
	 * Remove one ModelAction from the waiting list
	 * @return The ModelAction that was removed from the waiting list
	 */
	ModelAction * pop_wait_list() {
		ModelAction *ret = wait_list.front();
		wait_list.pop_back();
		return ret;
	}

	bool is_model_thread() { return model_thread; }

	friend void thread_startup();

	/**
	 * Intentionally NOT allocated with MODELALLOC or SNAPSHOTALLOC.
	 * Threads should be allocated on the user's normal (snapshotting) heap
	 * to allow their allocation/deallocation to follow the same pattern as
	 * the rest of the backtracked/replayed program.
	 */
private:
	int create_context();
	Thread *parent;
	ModelAction *creation;

	ModelAction *pending;
	void (*start_routine)(void *);
	void *arg;
	ucontext_t context;
	void *stack;
	thrd_t *user_thread;
	thread_id_t id;
	thread_state state;

	/**
	 * A list of ModelActions waiting on this Thread. Particularly, this
	 * list is used for thread joins, where another Thread waits for this
	 * Thread to complete
	 */
	std::vector< ModelAction *, SnapshotAlloc<ModelAction *> > wait_list;

	/**
	 * The value returned by the last action in this thread
	 * @see Thread::set_return_value()
	 * @see Thread::get_return_value()
	 */
	uint64_t last_action_val;

	/** @brief Is this Thread a special model-checker thread? */
	const bool model_thread;
};

Thread * thread_current();

static inline thread_id_t thrd_to_id(thrd_t t)
{
	return t;
}

static inline thread_id_t int_to_id(int i)
{
	return i;
}

static inline int id_to_int(thread_id_t id)
{
	return id;
}

#endif /* __THREADS_MODEL_H__ */
