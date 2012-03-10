#include <string.h>
#include <stdlib.h>

#include "libthreads.h"

#define STACK_SIZE (1024 * 1024)

static struct thread *current, *main_thread;

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
	t->context.uc_link = &main_thread->context;
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

static int thread_yield()
{
	struct thread *old, *next;

	DBG();
	if (current) {
		old = current;
		schedule_add_thread(old);
	} else {
		old = main_thread;
	}
	schedule_choose_next(&next);
	current = next;
	return thread_swap(old, next);
}

static void thread_dispose(struct thread *t)
{
	DEBUG("completed thread %d\n", current->index);
	t->completed = 1;
	stack_free(t->stack);
}

static void thread_wait_finish()
{
	struct thread *next;

	DBG();

	do {
		if (current)
			thread_dispose(current);
		schedule_choose_next(&next);
		current = next;
	} while (next && !thread_swap(main_thread, next));
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

	schedule_add_thread(t);
	return 0;
}

void thread_join(struct thread *t)
{
	while (!t->completed)
		thread_yield();
}

void a(int *parm)
{
	int i;

	for (i = 0; i < 10; i++) {
		printf("Thread %d, magic number %d, loop %d\n", current->index, *parm, i);
		if (i % 2)
			thread_yield();
	}
}

void user_main()
{
	struct thread t1, t2;
	int i = 17, j = 13;

	printf("%s() creating 2 threads\n", __func__);
	thread_create(&t1, &a, &i);
	thread_create(&t2, &a, &j);

	thread_join(&t1);
	thread_join(&t2);
	printf("%s() is finished\n", __func__);
}

int main()
{
	struct thread user_thread;

	main_thread = malloc(sizeof(struct thread));
	create_initial_thread(main_thread);

	/* Start user program */
	thread_create(&user_thread, &user_main, NULL);

	/* Wait for all threads to complete */
	thread_wait_finish();

	DEBUG("Exiting\n");
	return 0;
}
