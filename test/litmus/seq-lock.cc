#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <atomic>

std::atomic_int x;
std::atomic_int y;
std::atomic_int z;

static int N = 1;

static void a(void *obj)
{
	for (int i = 0; i < N; i++) {
		x.store(2 * i + 1, std::memory_order_release);
		y.store(i + 1, std::memory_order_release);
		z.store(i + 1, std::memory_order_release);
		x.store(2 * i + 2, std::memory_order_release);
	}
}

static void b(void *obj)
{
	printf("x: %d\n", x.load(std::memory_order_acquire));
	printf("y: %d\n", y.load(std::memory_order_acquire));
	printf("z: %d\n", z.load(std::memory_order_acquire));
	printf("x: %d\n", x.load(std::memory_order_acquire));
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	if (argc > 1)
		N = atoi(argv[1]);

	printf("N: %d\n", N);

	atomic_init(&x, 0);
	atomic_init(&y, 0);
	atomic_init(&z, 0);

	printf("Main thread: creating 2 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Main thread is finished\n");

	return 0;
}
