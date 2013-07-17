#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <atomic>

#include "librace.h"
#include "model-assert.h"

using namespace std;

atomic_int x, y;
atomic_intptr_t z, z2;

int r1, r2, r3; /* "local" variables */

/**
		This example illustrates a self-satisfying cycle involving
		synchronization.  A failed synchronization creates the store that
		causes the synchronization to fail.

		The C++11 memory model nominally allows r1=0, r2=1, r3=5.

		This example is insane, we don't support that behavior.
*/


static void a(void *obj)
{
	z.store((intptr_t)&y, memory_order_relaxed);
	r1 = y.fetch_add(1, memory_order_release);
	z.store((intptr_t)&x, memory_order_relaxed);
	r2 = y.fetch_add(1, memory_order_release);
}


static void b(void *obj)
{
	r3 = y.fetch_add(1, memory_order_acquire);
	intptr_t ptr = z.load(memory_order_relaxed);
	z2.store(ptr, memory_order_relaxed);
}

static void c(void *obj)
{
	atomic_int *ptr2 = (atomic_int *)z2.load(memory_order_relaxed);
	(*ptr2).store(5, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2, t3;

	atomic_init(&x, 0);
	atomic_init(&y, 0);
	atomic_init(&z, (intptr_t) &x);
	atomic_init(&z2, (intptr_t) &x);

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);

	printf("r1=%d, r2=%d, r3=%d\n", r1, r2, r3);

	return 0;
}
