#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>
#include "librace.h"
  atomic_int x1;
  atomic_int x2;
  atomic_int x3;
  atomic_int x4;
  atomic_int x5;
  atomic_int x6;
  atomic_int x7;
static void a(void *obj)
{
	atomic_store_explicit(&x1, 1,memory_order_relaxed);
}

static void b(void *obj)
{
	(void)atomic_load_explicit(&x1, memory_order_relaxed);
	atomic_store_explicit(&x2, 1,memory_order_relaxed);
}

static void c(void *obj)
{
	(void)atomic_load_explicit(&x2, memory_order_relaxed);
	atomic_store_explicit(&x3, 1,memory_order_relaxed);
}

static void d(void *obj)
{
	(void)atomic_load_explicit(&x3, memory_order_relaxed);
	atomic_store_explicit(&x4, 1,memory_order_relaxed);
}

static void e(void *obj)
{
	(void)atomic_load_explicit(&x4, memory_order_relaxed);
	atomic_store_explicit(&x5, 1,memory_order_relaxed);
}

static void f(void *obj)
{
	(void)atomic_load_explicit(&x5, memory_order_relaxed);
	atomic_store_explicit(&x6, 1,memory_order_relaxed);
}

static void g(void *obj)
{
	(void)atomic_load_explicit(&x6, memory_order_relaxed);
	atomic_store_explicit(&x7, 1,memory_order_relaxed);
}
static void h(void *obj)
{
	(void)atomic_load_explicit(&x7, memory_order_relaxed);
	(void)atomic_load_explicit(&x1, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2, t3, t4, t5, t6, t7, t8;
        atomic_init(&x1, 0);
        atomic_init(&x2, 0);
        atomic_init(&x3, 0);
        atomic_init(&x4, 0);
        atomic_init(&x5, 0);
        atomic_init(&x6, 0);
        atomic_init(&x7, 0);


        thrd_create(&t1, (thrd_start_t)&a, NULL);
        thrd_create(&t2, (thrd_start_t)&b, NULL);
        thrd_create(&t3, (thrd_start_t)&c, NULL);
        thrd_create(&t4, (thrd_start_t)&d, NULL);
        thrd_create(&t5, (thrd_start_t)&e, NULL);
        thrd_create(&t6, (thrd_start_t)&f, NULL);
        thrd_create(&t7, (thrd_start_t)&g, NULL);
        thrd_create(&t8, (thrd_start_t)&h, NULL);

        thrd_join(t1);
        thrd_join(t2);
        thrd_join(t3);
        thrd_join(t4);
        thrd_join(t5);
        thrd_join(t6);
        thrd_join(t7);
        thrd_join(t8);

        return 0;
}
