#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"
#include "model-assert.h"

atomic_int x;
atomic_int y;
atomic_llong z;
atomic_llong z2;

/** 
		This example illustrates a self-satisfying cycle involving
		synchronization.  A failed synchronization creates the store that
		causes the synchronization to fail. 
		
		The C++11 memory model nominally allows t1=0, t2=1, t3=5.

		This example is insane, we don't support that behavior.
*/


static void a(void *obj)
{
	atomic_store_explicit(&z, (long long) &y, memory_order_relaxed);
	int t1=atomic_fetch_add_explicit(&y, 1, memory_order_release);
	atomic_store_explicit(&z, (long long) &x, memory_order_relaxed);
	int t2=atomic_fetch_add_explicit(&y, 1, memory_order_release);
	printf("t1=%d, t2=%d\n", t1, t2);
}


static void b(void *obj)
{
	int t3=atomic_fetch_add_explicit(&y, 1, memory_order_acquire);
	void * ptr=(void *)atomic_load_explicit(&z, memory_order_relaxed);
	atomic_store_explicit(&z2, ptr, memory_order_relaxed);
	printf("t3=%d\n", t3);
}

static void c(void *obj)
{
	void * ptr2=(void *) atomic_load_explicit(&z2, memory_order_relaxed);
	atomic_store_explicit((atomic_int *)ptr2, 5, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2, t3;

	
	atomic_init(&x, 0);
	atomic_init(&y, 0);
	atomic_init(&z, (long long) &x);
	atomic_init(&z2, (long long) &x);

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);
	
	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	

	return 0;
}
