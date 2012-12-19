/*
 * This test performs some relaxed, release, acquire opeations on a single
 * atomic variable. It can give some rough idea of release sequence support but
 * probably should be improved to give better information.
 *
 * This test tries to establish two release sequences, where we should always
 * either establish both or establish neither. (Note that this is only true for
 * a few executions of interest, where both load-acquire's read from the same
 * write.)
 */

#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_int x;
int var = 0;

static void a(void *obj)
{
	store_32(&var, 1);
	atomic_store_explicit(&x, 1, memory_order_release);
	atomic_store_explicit(&x, 42, memory_order_relaxed);
}

static void b(void *obj)
{
	int r = atomic_load_explicit(&x, memory_order_acquire);
	printf("r = %d\n", r);
	printf("load %d\n", load_32(&var));
}

static void c(void *obj)
{
	atomic_store_explicit(&x, 2, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2, t3, t4;

	atomic_init(&x, 0);

	printf("Main thread: creating 4 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&b, NULL);
	thrd_create(&t4, (thrd_start_t)&c, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	thrd_join(t4);
	printf("Main thread is finished\n");

	return 0;
}
