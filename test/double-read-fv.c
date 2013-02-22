/*
 * Try to read the same value as a future value twice.
 *
 * This test should be able to see r1 = r2 = 42. Currently, we never see that
 * (as of 2/21/13) because the r2 load won't have a potential future value of
 * 42 at the same time as r1, due to our scheduling (the loads for r1 and r2
 * must occur before the write of x = 42).
 *
 * Note that the atomic_int y is simply used to aid in forcing a particularly
 * interesting scheduling. It is superfluous.
 */
#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_int x;
atomic_int y;

static void a(void *obj)
{
	int r1 = atomic_load_explicit(&x, memory_order_relaxed);
	int r2 = atomic_load_explicit(&x, memory_order_relaxed);
	printf("r1 = %d, r2 = %d\n", r1, r2);
}

static void b(void *obj)
{
	atomic_store_explicit(&y, 43, memory_order_relaxed);
	atomic_store_explicit(&x, 42, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	printf("Main thread: creating 2 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Main thread is finished\n");

	return 0;
}
