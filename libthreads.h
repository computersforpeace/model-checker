#ifndef __LIBTHREADS_H__
#define __LIBTHREADS_H__

#include <ucontext.h>

typedef enum thread_state {
	THREAD_CREATED,
	THREAD_RUNNING,
	THREAD_READY,
	THREAD_COMPLETED
} thread_state;

typedef int thread_id_t;

struct thread {
	void (*start_routine)();
	void *arg;
	ucontext_t context;
	void *stack;
	thread_id_t id;
	thread_state state;
};

int thread_create(struct thread *t, void (*start_routine)(), void *arg);
void thread_join(struct thread *t);
int thread_yield(void);
struct thread *thread_current(void);

extern void user_main(void);

#endif /* __LIBTHREADS_H__ */
