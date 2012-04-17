#ifndef __THREADS_H__
#define __THREADS_H__

#include <ucontext.h>

#include "libthreads.h"

typedef enum thread_state {
	THREAD_CREATED,
	THREAD_RUNNING,
	THREAD_READY,
	THREAD_COMPLETED
} thread_state;

class ModelAction;

class Thread {
public:
	Thread(thrd_t *t, void (*func)(), void *a);
	Thread(thrd_t *t);
	int swap(Thread *t);
	void dispose();

	thread_state get_state() { return state; }
	void set_state(thread_state s) { state = s; }
	thread_id_t get_id();
	void set_id(thread_id_t i) { *user_thread = i; }
	thrd_t get_thrd_t() { return *user_thread; }
private:
	int create_context();

	void (*start_routine)();
	void *arg;
	ucontext_t context;
	void *stack;
	thrd_t *user_thread;
	thread_state state;
};

Thread *thread_current();

static inline thread_id_t thrd_to_id(thrd_t t)
{
	return t;
}

#endif /* __THREADS_H__ */
