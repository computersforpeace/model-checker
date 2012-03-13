#include <string.h>
#include <stdlib.h>

#include "libthreads.h"
#include "schedule.h"
#include "common.h"

/* global "model" struct */
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
	return create_context(t);
}

static int thread_swap(struct thread *old, struct thread *new)
{
	return swapcontext(&old->context, &new->context);
}

int thread_yield(void)
{
	struct thread *old, *next;

	DBG();
	old = thread_current();
	model->scheduler->add_thread(old);
	next = model->scheduler->next_thread();
	DEBUG("(%d, %d)\n", old->index, next->index);
	return thread_swap(old, next);
}

static void thread_dispose(struct thread *t)
{
	DEBUG("completed thread %d\n", thread_current()->index);
	t->state = THREAD_COMPLETED;
	stack_free(t->stack);
}

static void thread_wait_finish(void)
{
	struct thread *curr, *next;

	DBG();

	do {
		if ((curr = thread_current()))
			thread_dispose(curr);
		next = model->scheduler->next_thread();
	} while (next && !thread_swap(model->system_thread, next));
}

int thread_create(struct thread *t, void (*start_routine), void *arg)
{
	static int created = 1;
	int ret = 0;

	DBG();

	memset(t, 0, sizeof(*t));
	t->index = created++;
	DEBUG("create thread %d\n", t->index);

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

void thread_join(struct thread *t)
{
	while (t->state != THREAD_COMPLETED)
		thread_yield();
}

struct thread *thread_current(void)
{
	return model->scheduler->get_current_thread();
}

int main()
{
	struct thread user_thread;
	struct thread *main_thread;

	model_checker_init();

	main_thread = malloc(sizeof(struct thread));
	create_initial_thread(main_thread);
	model_checker_add_system_thread(main_thread);

	/* Start user program */
	thread_create(&user_thread, &user_main, NULL);

	/* Wait for all threads to complete */
	thread_wait_finish();

	DEBUG("Exiting\n");
	return 0;
}
