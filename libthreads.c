#include <string.h>
#include <stdlib.h>

//#define CONFIG_DEBUG

#include "libthreads.h"

#define STACK_SIZE (1024 * 1024)

static struct thread *current;

static void *stack_allocate(size_t size)
{
	return malloc(size);
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
	t->context.uc_link = &current->context;
	makecontext(&t->context, t->start_routine, 1, t->arg);

	return 0;
}

static int create_initial_thread(struct thread *t)
{
	memset(t, 0, sizeof(*t));
	return create_context(t);
}

int thread_create(struct thread *t, void (*start_routine), void *arg)
{
	static int created = 1;

	DBG();

	memset(t, 0, sizeof(*t));
	t->index = created++;
	DEBUG("create thread %d\n", t->index);

	t->start_routine = start_routine;
	t->arg = arg;

	/* Initialize state */
	return create_context(t);
}

void thread_start(struct thread *t)
{
	struct thread *old = current;
	DBG();

	current = t;
	swapcontext(&old->context, &current->context);

	DBG();
}

void a(int *idx)
{
	int i;

	for (i = 0; i < 10; i++)
		printf("Thread %d, loop %d\n", *idx, i);
}

void user_main()
{
	struct thread t1, t2;
	int i = 2, j = 3;

	thread_create(&t1, &a, &i);
	thread_create(&t2, &a, &j);

	printf("%s() is going to start 1 thread\n", __func__);
	thread_start(&t1);
	thread_start(&t2);
	printf("%s() is finished\n", __func__);
}

int main()
{
	struct thread main_thread, user_thread;

	create_initial_thread(&main_thread);
	current = &main_thread;

	thread_create(&user_thread, &user_main, NULL);
	thread_start(&user_thread);

	DBG();

	DEBUG("Exiting\n");
	return 0;
}
