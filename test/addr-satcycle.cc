/**
 * @file addr-satcycle.cc
 * @brief Address-based satisfaction cycle test
 *
 * This program has a peculiar behavior which is technically legal under the
 * current C++ memory model but which is a result of a type of satisfaction
 * cycle. We use this as justification for part of our modifications to the
 * memory model when proving our model-checker's correctness.
 */

#include <atomic>
#include <threads.h>
#include <stdio.h>

#include "model-assert.h"

using namespace std;

atomic_int x[2], idx, y;

int r1, r2, r3; /* "local" variables */

static void a(void *obj)
{
	r1 = idx.load(memory_order_relaxed);
	x[r1].store(0, memory_order_relaxed);

	/* Key point: can we guarantee that &x[0] == &x[r1]? */
	r2 = x[0].load(memory_order_relaxed);
	y.store(r2);
}

static void b(void *obj)
{
	r3 = y.load(memory_order_relaxed);
	idx.store(r3, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	atomic_init(&x[0], 1);
	atomic_init(&idx, 0);
	atomic_init(&y, 0);

	printf("Main thread: creating 2 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Main thread is finished\n");

	printf("r1 = %d\n", r1);
	printf("r2 = %d\n", r2);
	printf("r3 = %d\n", r3);

	/*
	 * This condition should not be hit because it only occurs under a
	 * satisfaction cycle
	 */
	bool cycle = (r1 == 1 && r2 == 1 && r3 == 1);
	MODEL_ASSERT(!cycle);

	return 0;
}
