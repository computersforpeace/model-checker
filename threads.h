/** @file threads.h
 *  @brief Model Checker Thread class.
 */

#ifndef __THREADS_H__
#define __THREADS_H__

#include <ucontext.h>
#include "mymemory.h"
#include "libthreads.h"

typedef int thread_id_t;

#define THREAD_ID_T_NONE	-1

/** @brief Represents the state of a user Thread */
typedef enum thread_state {
	/** Thread was just created and hasn't run yet */
	THREAD_CREATED,
	/** Thread is running */
	THREAD_RUNNING,
	/**
	 * Thread has yielded to the model-checker but is ready to run. Used
	 * during an action that caused a context switch to the model-checking
	 * context.
	 */
	THREAD_READY,
	/** Thread has completed its execution */
	THREAD_COMPLETED
} thread_state;

class ModelAction;

/** @brief A Thread is created for each user-space thread */
class Thread {
public:
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
	void set_return_value(int value) { last_action_val = value; }

	/**
	 * Retrieve a return value for the last action in this thread. Used,
	 * for instance, for an atomic read to return the 'read' value. Should
	 * be called from a user context.
	 * @return The value 'returned' by the action
	 */
	int get_return_value() { return last_action_val; }

	friend void thread_startup();

	SNAPSHOTALLOC
private:
	int create_context();
	Thread *parent;
	ModelAction *creation;

	void (*start_routine)(void *);
	void *arg;
	ucontext_t context;
	void *stack;
	thrd_t *user_thread;
	thread_id_t id;
	thread_state state;

	/**
	 * The value returned by the last action in this thread
	 * @see Thread::set_return_value()
	 * @see Thread::get_return_value()
	 */
	int last_action_val;
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

#endif /* __THREADS_H__ */
