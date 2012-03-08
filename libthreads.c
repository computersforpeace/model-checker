#include <string.h>
#include <stdlib.h>
#include <ucontext.h>

//#define CONFIG_DEBUG

#include "libthreads.h"

#define STACK_SIZE (1024 * 1024)

static struct thread *current;
static ucontext_t *cleanup;

static void *stack_allocate(size_t size)
{
	return malloc(size);
}

int thread_create(struct thread *t, void (*start_routine), void *arg)
{
	static int created;
	ucontext_t local;

	DBG();

	t->index = created++;
	DEBUG("create thread %d\n", t->index);

	t->start_routine = start_routine;
	t->arg = arg;

	/* Initialize state */
	getcontext(&t->context);
	t->stack = stack_allocate(STACK_SIZE);
	t->context.uc_stack.ss_sp = t->stack;
	t->context.uc_stack.ss_size = STACK_SIZE;
	t->context.uc_stack.ss_flags = 0;
	if (current)
		t->context.uc_link = &current->context;
	else
		t->context.uc_link = cleanup;
	makecontext(&t->context, t->start_routine, 1, t->arg);

	return 0;
}

void thread_start(struct thread *t)
{
	DBG();

	if (current) {
		struct thread *old = current;
		current = t;
		swapcontext(&old->context, &current->context);
	} else {
		current = t;
		swapcontext(cleanup, &current->context);
	}
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
	int i = 1, j = 2;

	thread_create(&t1, &a, &i);
	thread_create(&t2, &a, &j);

	printf("user_main() is going to start 2 threads\n");
	thread_start(&t1);
	thread_start(&t2);
	printf("user_main() is finished\n");
}

int main()
{
	struct thread t;
	ucontext_t main_context;

	cleanup = &main_context;

	thread_create(&t, &user_main, NULL);

	thread_start(&t);

	DBG();

	DEBUG("Exiting\n");
	return 0;
}
