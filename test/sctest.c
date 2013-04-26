#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_int x;
atomic_int y;
atomic_int z;

static void a(void *obj)
{
	atomic_store_explicit(&z, 1, memory_order_relaxed);
}

static void b(void *obj)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	atomic_store_explicit(&y, 1, memory_order_relaxed);
	int r1=atomic_load_explicit(&z, memory_order_relaxed);
}
static void c(void *obj)
{
	atomic_store_explicit(&z, 2, memory_order_relaxed);
	atomic_store_explicit(&x, 2, memory_order_relaxed);
	int r2=atomic_load_explicit(&y, memory_order_relaxed);
}

static void d(void *obj)
{
	atomic_store_explicit(&z, 3, memory_order_relaxed);
	atomic_store_explicit(&y, 2, memory_order_relaxed);
	int r3=atomic_load_explicit(&x, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2,t3, t4;

	atomic_init(&x, 0);
	atomic_init(&y, 0);
	atomic_init(&z, 0);

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);
	thrd_create(&t4, (thrd_start_t)&d, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	thrd_join(t4);

	return 0;
}
