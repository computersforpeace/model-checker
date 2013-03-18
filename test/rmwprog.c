#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"
#include "model-assert.h"

atomic_int x;
static int N = 2;

static void a(void *obj)
{
	int i;
	for (i = 0; i < N; i++)
		atomic_fetch_add_explicit(&x, 1, memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;

	if (argc > 1)
		N = atoi(argv[1]);

	atomic_init(&x, 0);
	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&a, NULL);

	thrd_join(t1);
	thrd_join(t2);

	MODEL_ASSERT(atomic_load(&x) == N * 2);

	return 0;
}
