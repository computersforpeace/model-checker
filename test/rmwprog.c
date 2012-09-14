#include <stdio.h>

#include "libthreads.h"
#include "librace.h"
#include "stdatomic.h"

atomic_int x;

static void a(void *obj)
{
	atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
	atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
}

void user_main()
{
	thrd_t t1, t2;

	atomic_init(&x, 0);
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&a, NULL);

	thrd_join(t1);
	thrd_join(t2);
}
