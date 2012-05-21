#include "libthreads.h"
#include "common.h"
#include "threads.h"

/* global "model" object */
#include "model.h"

#define STACK_SIZE (1024 * 1024)

static void * stack_allocate(size_t size)
{
	return userMalloc(size);
}

static void stack_free(void *stack)
{
	userFree(stack);
}

Thread * thread_current(void)
{
	return model->scheduler->get_current_thread();
}

/* This method just gets around makecontext not being 64-bit clean */

void thread_startup() {
	Thread * curr_thread=thread_current();
	curr_thread->start_routine(curr_thread->arg);
}

int Thread::create_context()
{
	int ret;

	ret = getcontext(&context);
	if (ret)
		return ret;

	/* Initialize new managed context */
	stack = stack_allocate(STACK_SIZE);
	context.uc_stack.ss_sp = stack;
	context.uc_stack.ss_size = STACK_SIZE;
	context.uc_stack.ss_flags = 0;
	context.uc_link = model->get_system_context();
	makecontext(&context, thread_startup, 0);

	return 0;
}

int Thread::swap(Thread *t, ucontext_t *ctxt)
{
	return swapcontext(&t->context, ctxt);
}

int Thread::swap(ucontext_t *ctxt, Thread *t)
{
	return swapcontext(ctxt, &t->context);
}

void Thread::complete()
{
	if (state != THREAD_COMPLETED) {
		DEBUG("completed thread %d\n", get_id());
		state = THREAD_COMPLETED;
		if (stack)
			stack_free(stack);
	}
}

Thread::Thread(thrd_t *t, void (*func)(void *), void *a) {
	int ret;

	user_thread = t;
	start_routine = func;
	arg = a;

	/* Initialize state */
	ret = create_context();
	if (ret)
		printf("Error in create_context\n");

	state = THREAD_CREATED;
	id = model->get_next_id();
	*user_thread = id;
	parent = thread_current();
}

Thread::~Thread()
{
	complete();
	model->remove_thread(this);
}

thread_id_t Thread::get_id()
{
	return id;
}
