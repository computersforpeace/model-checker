#include <string.h>
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

static int create_context(struct thread *t)
{
	int ret;

	memset(&t->context, 0, sizeof(t->context));
	ret = getcontext(&t->context);
	if (ret)
		return ret;

	/* t->start_routine == NULL means this is our initial context */
	if (!t->start_routine)
		return 0;

	/* Initialize new managed context */
	t->stack = stack_allocate(STACK_SIZE);
	t->context.uc_stack.ss_sp = t->stack;
	t->context.uc_stack.ss_size = STACK_SIZE;
	t->context.uc_stack.ss_flags = 0;
	t->context.uc_link = &model->system_thread->context;
	makecontext(&t->context, t->start_routine, 1, t->arg);

	return 0;
}

static int create_initial_thread(struct thread *t)
{
	memset(t, 0, sizeof(*t));
	model->assign_id(t);
	return create_context(t);
}

static int thread_swap(struct thread *t1, struct thread *t2)
{
	return swapcontext(&t1->context, &t2->context);
}

static void thread_dispose(struct thread *t)
{
	DEBUG("completed thread %d\n", thread_current()->id);
	t->state = THREAD_COMPLETED;
	stack_free(t->stack);
}

/*
 * Return 1 if found next thread, 0 otherwise
 */
static int thread_system_next(void)
{
	struct thread *curr, *next;

	curr = thread_current();
	model->check_current_action();
	if (curr) {
		if (curr->state == THREAD_READY)
			model->scheduler->add_thread(curr);
		else if (curr->state == THREAD_RUNNING)
			/* Stopped while running; i.e., completed */
			thread_dispose(curr);
		else
			DEBUG("ERROR: current thread in unexpected state??\n");
	}
	next = model->scheduler->next_thread();
	if (next)
		next->state = THREAD_RUNNING;
	DEBUG("(%d, %d)\n", curr ? curr->id : -1, next ? next->id : -1);
	if (!next)
		return 1;
	return thread_swap(model->system_thread, next);
}

static void thread_wait_finish(void)
{

	DBG();

	while (!thread_system_next());
}

int thread_switch_to_master(ModelAction *act)
{
	struct thread *old, *next;

	DBG();
	model->set_current_action(act);
	old = thread_current();
	old->state = THREAD_READY;
	next = model->system_thread;
	return thread_swap(old, next);
}

/*
 * User program API functions
 */
int thread_create(struct thread *t, void (*start_routine)(), void *arg)
{
	int ret = 0;

	DBG();

	memset(t, 0, sizeof(*t));
	model->assign_id(t);
	DEBUG("create thread %d\n", t->id);

	t->start_routine = start_routine;
	t->arg = arg;

	/* Initialize state */
	ret = create_context(t);
	if (ret)
		return ret;

	t->state = THREAD_CREATED;

	model->scheduler->add_thread(t);
	return 0;
}

int thread_join(struct thread *t)
{
	int ret = 0;
	while (t->state != THREAD_COMPLETED && !ret)
		/* seq_cst is just a 'don't care' parameter */
		ret = thread_switch_to_master(new ModelAction(THREAD_JOIN, memory_order_seq_cst, NULL, VALUE_NONE));
	return ret;
}

int thread_yield(void)
{
	/* seq_cst is just a 'don't care' parameter */
	return thread_switch_to_master(new ModelAction(THREAD_YIELD, memory_order_seq_cst, NULL, VALUE_NONE));
}

struct thread *thread_current(void)
{
	return model->scheduler->get_current_thread();
}

/*
 * Main system function
 */
int main()
{
	struct thread user_thread;
	struct thread *main_thread;

	model = new ModelChecker();

	main_thread = (struct thread *)myMalloc(sizeof(*main_thread));
	create_initial_thread(main_thread);
	model->add_system_thread(main_thread);

	/* Start user program */
	thread_create(&user_thread, &user_main, NULL);

	/* Wait for all threads to complete */
	thread_wait_finish();

	model->print_trace();
	delete model;
	myFree(main_thread);

	DEBUG("Exiting\n");
	return 0;
}
