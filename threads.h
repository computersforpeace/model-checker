#ifndef __THREADS_H__
#define __THREADS_H__

#include <ucontext.h>

#include "libthreads.h"

#define THREAD_ID_T_NONE	-1

typedef enum thread_state {
	THREAD_CREATED,
	THREAD_RUNNING,
	THREAD_READY,
	THREAD_COMPLETED
} thread_state;

class Thread {
public:
	void * operator new(size_t size);
	void operator delete(void *ptr);
	Thread(thrd_t *t, void (*func)(), void *a);
	Thread(thrd_t *t);
	~Thread();
	void complete();

	static int swap(ucontext_t *ctxt, Thread *t);
	static int swap(Thread *t, ucontext_t *ctxt);

	thread_state get_state() { return state; }
	void set_state(thread_state s) { state = s; }
	thread_id_t get_id();
	thrd_t get_thrd_t() { return *user_thread; }
private:
	int create_context();

	void (*start_routine)();
	void *arg;
	ucontext_t context;
	void *stack;
	thrd_t *user_thread;
	thread_id_t id;
	thread_state state;
};

Thread * thread_current();

static inline thread_id_t thrd_to_id(thrd_t t)
{
	return t;
}

#endif /* __THREADS_H__ */
