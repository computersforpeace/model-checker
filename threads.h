#ifndef __THREADS_H__
#define __THREADS_H__

#include <ucontext.h>
#include "mymemory.h"
#include "libthreads.h"

typedef int thread_id_t;

#define THREAD_ID_T_NONE	-1

typedef enum thread_state {
	THREAD_CREATED,
	THREAD_RUNNING,
	THREAD_READY,
	THREAD_COMPLETED
} thread_state;

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

	friend void thread_startup();

	MEMALLOC
private:
	int create_context();
	Thread *parent;

	void (*start_routine)(void *);
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

static inline thread_id_t int_to_id(int i)
{
	return i;
}

static inline int id_to_int(thread_id_t id)
{
	return id;
}

#endif /* __THREADS_H__ */
