#include <stdio.h>

#include "libthreads.h"
#include "libatomic.h"

static void a(atomic_int *obj)
{
	int i;

	for (i = 0; i < 10; i++) {
		printf("Thread %d, loop %d\n", thread_current()->id, i);
		if (i % 2)
			atomic_load(obj);
	}
}

void user_main()
{
	struct thread t1, t2;
	atomic_int obj;

	printf("%s() creating 2 threads\n", __func__);
	thread_create(&t1, (void (*)())&a, &obj);
	thread_create(&t2, (void (*)())&a, &obj);

	thread_join(&t1);
	thread_join(&t2);
	printf("%s() is finished\n", __func__);
}
