#include <stdio.h>
#include <stdlib.h>

#include "libthreads.h"
#include "libatomic.h"
#include "librace.h"

static void a(atomic_int *obj)
{
	int i;
	int ret;

	store_32(&i, 10);
	printf("load 32 yields: %d\n", load_32(&i));

	for (i = 0; i < 2; i++) {
		printf("Thread %d, loop %d\n", thrd_current(), i);
		switch (i % 2) {
		case 1:
			ret = atomic_load(obj);
			printf("Read value: %d\n", ret);
			break;
		case 0:
			atomic_store(obj, i + 1);
			printf("Write value: %d\n", i + 1);
			break;
		}
	}
}

void user_main()
{
	thrd_t t1, t2;
	atomic_int *obj;

	obj = malloc(sizeof(*obj));

	atomic_init(obj, 0);

	printf("Thread %d: creating 2 threads\n", thrd_current());
	thrd_create(&t1, (thrd_start_t)&a, obj);
	thrd_create(&t2, (thrd_start_t)&a, obj);

	thrd_join(t1);
	thrd_join(t2);
	free(obj);
	printf("Thread %d is finished\n", thrd_current());
}
