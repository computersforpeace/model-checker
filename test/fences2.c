#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"
#include "model-assert.h"

atomic_int x;
atomic_int y;

static void a(void *obj)
{
	atomic_store_explicit(&x, 1, memory_order_relaxed);
	atomic_thread_fence(memory_order_release);
	atomic_store_explicit(&x, 2, memory_order_relaxed);
}

static void b(void *obj)
{
	int r1, r2;
	r1 = atomic_load_explicit(&x, memory_order_relaxed);
	atomic_thread_fence(memory_order_acquire);
	r2 = atomic_load_explicit(&x, memory_order_relaxed);

	printf("FENCES: r1 = %d, r2 = %d\n", r1, r2);
	if (r1 == 2)
		MODEL_ASSERT(r2 != 1);
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
	printf("Main thread is finishing\n");

	return 0;
}
