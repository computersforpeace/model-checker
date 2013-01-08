#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_int x;
atomic_int y;

static void a(void *obj)
{
	int v1=atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
	int v2=atomic_fetch_add_explicit(&y, 1, memory_order_relaxed);
	printf("v1 = %d, v2=%d\n", v1, v2);
}

static void b(void *obj)
{
	int v3=atomic_fetch_add_explicit(&y, 1, memory_order_relaxed);
	int v4=atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
	printf("v3 = %d, v4=%d\n", v3, v4);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	atomic_init(&x, 0);
	atomic_init(&y, 0);
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);

	return 0;
}
