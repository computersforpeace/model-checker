#include <stdio.h>

#include "libthreads.h"
#include "librace.h"
#include "stdatomic.h"

atomic_int x;
atomic_int y;

static void a(void *obj)
{
	int r1=atomic_load_explicit(&y, memory_order_relaxed);
	atomic_store_explicit(&x, r1, memory_order_relaxed);
	printf("r1=%u\n",r1);
}

static void b(void *obj)
{
	int r2=atomic_load_explicit(&x, memory_order_relaxed);
	atomic_store_explicit(&y, 42, memory_order_relaxed);
	printf("r2=%u\n",r2);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	atomic_init(&x, 0);
	atomic_init(&y, 0);

	printf("Thread %d: creating 2 threads\n", thrd_current());
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Thread %d is finished\n", thrd_current());

	return 0;
}
