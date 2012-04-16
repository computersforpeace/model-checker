#include <stdlib.h>

#include "libthreads.h"
#include "schedule.h"
#include "common.h"
#include "threads_internal.h"

/* global "model" object */
#include "model.h"

#define STACK_SIZE (1024 * 1024)

static void *stack_allocate(size_t size)
{
	return malloc(size);
}

static void stack_free(void *stack)
{
	free(stack);
}

Thread *thread_current(void)
{
	return model->scheduler->get_current_thread();
}

int Thread::create_context()
{
	int ret;

	ret = getcontext(&context);
	if (ret)
		return ret;

	/* start_routine == NULL means this is our initial context */
	if (!start_routine)
		return 0;

	/* Initialize new managed context */
	stack = stack_allocate(STACK_SIZE);
	context.uc_stack.ss_sp = stack;
	context.uc_stack.ss_size = STACK_SIZE;
	context.uc_stack.ss_flags = 0;
	context.uc_link = &model->system_thread->context;
	makecontext(&context, start_routine, 1, arg);

	return 0;
}

int Thread::swap(Thread *t)
{
	return swapcontext(&this->context, &t->context);
}

void Thread::dispose()
{
	DEBUG("completed thread %d\n", thread_current()->get_id());
	state = THREAD_COMPLETED;
	stack_free(stack);
}

int Thread::switch_to_master(ModelAction *act)
{
	Thread *next;

	DBG();
	model->set_current_action(act);
	state = THREAD_READY;
	next = model->system_thread;
	return swap(next);
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
	model->assign_id(this);
	model->scheduler->add_thread(this);
}

Thread::Thread(thrd_t *t) {
	/* system thread */
	user_thread = t;
	state = THREAD_CREATED;
	model->assign_id(this);
	create_context();
	model->add_system_thread(this);
}

thread_id_t Thread::get_id()
{
	return thrd_to_id(*user_thread);
}

/*
 * Return 1 if found next thread, 0 otherwise
 */
static int thread_system_next(void)
{
	Thread *curr, *next;

	curr = thread_current();
	model->check_current_action();
	if (curr) {
		if (curr->get_state() == THREAD_READY)
			model->scheduler->add_thread(curr);
		else if (curr->get_state() == THREAD_RUNNING)
			/* Stopped while running; i.e., completed */
			curr->dispose();
		else
			DEBUG("ERROR: current thread in unexpected state??\n");
	}
	next = model->scheduler->next_thread();
	if (next)
		next->set_state(THREAD_RUNNING);
	DEBUG("(%d, %d)\n", curr ? curr->get_id() : -1, next ? next->get_id() : -1);
	if (!next)
		return 1;
	return model->system_thread->swap(next);
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
	thrd_t user_thread, main_thread;
	Thread *th;

	model = new ModelChecker();

	th = new Thread(&main_thread);

	/* Start user program */
	thrd_create(&user_thread, &user_main, NULL);

	/* Wait for all threads to complete */
	thread_wait_finish();

	model->print_trace();
	delete th;
	delete model;

	DEBUG("Exiting\n");
	return 0;
}
