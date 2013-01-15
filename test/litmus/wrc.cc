#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <atomic>

static int N = 2;

std::atomic_int *x;

static void a(void *obj)
{
	int idx = *((int *)obj);

	if (idx > 0)
		x[idx - 1].load(std::memory_order_relaxed);

	if (idx < N)
		x[idx].store(1, std::memory_order_relaxed);
	else
		x[0].load(std::memory_order_relaxed);
}

int user_main(int argc, char **argv)
{
	thrd_t *threads;
	int *indexes;

	if (argc > 1)
		N = atoi(argv[1]);
	if (N < 2) {
		printf("Error: must have N >= 2\n");
		return 1;
	}
	printf("N: %d\n", N);

	threads = (thrd_t *)malloc((N + 1) * sizeof(thrd_t));
	x = (std::atomic_int *)malloc(N * sizeof(std::atomic_int));
	indexes = (int *)malloc((N + 1) * sizeof(int));
	
	for (int i = 0; i < N + 1; i++)
		indexes[i] = i;

	for (int i = 0; i < N; i++)
		atomic_init(&x[i], 0);

	for (int i = 0; i < N + 1; i++)
		thrd_create(&threads[i], (thrd_start_t)&a, (void *)&indexes[i]);

	for (int i = 0; i < N + 1; i++)
		thrd_join(threads[i]);

        return 0;
}
