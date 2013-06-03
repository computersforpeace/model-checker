#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"

atomic_int a;
atomic_int b;

static void r(void *obj)
{
	int r1=atomic_load_explicit(&a, memory_order_relaxed);
	int r2=atomic_load_explicit(&a, memory_order_relaxed);
	if (r1==r2)
		atomic_store_explicit(&b, 2, memory_order_relaxed);
	printf("r1=%d\n",r1);
	printf("r2=%d\n",r2);
}

static void s(void *obj)
{
	int r3=atomic_load_explicit(&b, memory_order_relaxed);
	atomic_store_explicit(&a, r3, memory_order_relaxed);
	printf("r3=%d\n",r3);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	atomic_init(&a, 0);
	atomic_init(&b, 1);

	printf("Main thread: creating 2 threads\n");
	thrd_create(&t1, (thrd_start_t)&r, NULL);
	thrd_create(&t2, (thrd_start_t)&s, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Main thread is finished\n");

	return 0;
}
