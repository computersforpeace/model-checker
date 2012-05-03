#include <stdlib.h>

#include "libthreads.h"
#include "schedule.h"
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
	makecontext(&context, start_routine, 1, arg);

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

void * Thread::operator new(size_t size) {
	return userMalloc(size);
}

void Thread::operator delete(void *ptr) {
	userFree(ptr);
}

Thread::Thread(thrd_t *t, void (*func)(), void *a) {
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

/*
 * Return 1 if found next thread, 0 otherwise
 */
static int thread_system_next(void)
{
	Thread *curr, *next;

	curr = thread_current();
	if (curr) {
		if (curr->get_state() == THREAD_READY) {
			model->check_current_action();
			model->scheduler->add_thread(curr);
		} else if (curr->get_state() == THREAD_RUNNING)
			/* Stopped while running; i.e., completed */
			curr->complete();
		else
			ASSERT(false);
	}
	next = model->scheduler->next_thread();
	if (next)
		next->set_state(THREAD_RUNNING);
	DEBUG("(%d, %d)\n", curr ? curr->get_id() : -1, next ? next->get_id() : -1);
	if (!next)
		return 1;
	return Thread::swap(model->get_system_context(), next);
}

static void thread_wait_finish(void)
{

	DBG();

	while (!thread_system_next());
}

/*
 * Main system function
 */
int main()
{
	thrd_t user_thread;
	ucontext_t main_context;

	model = new ModelChecker();

	if (getcontext(&main_context))
		return 1;

	model->set_system_context(&main_context);

	do {
		/* Start user program */
		model->add_thread(new Thread(&user_thread, &user_main, NULL));

		/* Wait for all threads to complete */
		thread_wait_finish();
	} while (model->next_execution());

	delete model;

	DEBUG("Exiting\n");
	return 0;
}
