#include <stdio.h>
#include <threads.h>
#include <mutex>

#include "librace.h"

std::mutex *x;
std::mutex *y;
uint32_t shared = 0;

static void a(void *obj)
{
	x->lock();
	y->lock();
	printf("shared = %u\n", load_32(&shared));
	y->unlock();
	x->unlock();
}

static void b(void *obj)
{
	y->lock();
	x->lock();
	store_32(&shared, 16);
	printf("write shared = 16\n");
	x->unlock();
	y->unlock();
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	x = new std::mutex();
	y = new std::mutex();

	printf("Main thread: creating 2 threads\n");
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	printf("Main thread is finished\n");

	return 0;
}
