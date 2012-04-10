#include <stdio.h>

#include "libthreads.h"
#include "libatomic.h"

static void a(atomic_int *obj)
{
	int i;

	for (i = 0; i < 10; i++) {
		printf("Thread %d, loop %d\n", thread_current()->id, i);
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
	struct thread t1, t2;
	atomic_int obj;

	printf("Thread %d creating 2 threads\n", thread_current()->id);
	thread_create(&t1, (void (*)())&a, &obj);
	thread_create(&t2, (void (*)())&a, &obj);

	thread_join(&t1);
	thread_join(&t2);
	printf("Thread %d is finished\n", thread_current()->id);
}
