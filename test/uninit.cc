/**
 * @file uninit.cc
 * @brief Uninitialized loads test
 *
 * This is a test of the "uninitialized loads" code. While we don't explicitly
 * initialize y, this example's synchronization pattern should guarantee we
 * never see it uninitialized.
 */
#include <stdio.h>
#include <threads.h>
#include <atomic>

#include "librace.h"

std::atomic_int x;
std::atomic_int y;

static void a(void *obj)
{
	int flag = x.load(std::memory_order_acquire);
	printf("flag: %d\n", flag);
	if (flag == 2)
		printf("Load: %d\n", y.load(std::memory_order_relaxed));
}

static void b(void *obj)
{
	printf("fetch_add: %d\n", x.fetch_add(1, std::memory_order_relaxed));
}

static void c(void *obj)
{
	y.store(3, std::memory_order_relaxed);
	x.store(1, std::memory_order_release);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2, t3;

	std::atomic_init(&x, 0);

	printf("Main thread: creating 3 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);
	thrd_create(&t3, (thrd_start_t)&c, NULL);

	thrd_join(t1);
	thrd_join(t2);
	thrd_join(t3);
	printf("Main thread is finished\n");

	return 0;
}
