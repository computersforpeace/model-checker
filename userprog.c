#include <stdio.h>

#include "libthreads.h"
#include "libatomic.h"

static void a(atomic_int *obj)
{
	int i;

	for (i = 0; i < 10; i++) {
		printf("Thread %d, loop %d\n", thrd_current(), i);
		switch (i % 4) {
		case 1:
			atomic_load(obj);
			break;
		case 3:
			atomic_store(obj, i);
			break;
		}
	}
}

void user_main()
{
	thrd_t t1, t2;
	atomic_int obj;

	printf("Creating 2 threads\n");
	thrd_create(&t1, (void (*)())&a, &obj);
	thrd_create(&t2, (void (*)())&a, &obj);

	thrd_join(t1);
	thrd_join(t2);
	printf("Thread is finished\n");
}
