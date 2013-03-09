#include <stdlib.h>
#include <stdio.h>
#include <threads.h>
#include <atomic>

static int N = 2;

/* Can be tested for different behavior with relaxed vs. release/acquire/seq-cst */
#define load_mo std::memory_order_relaxed
#define store_mo std::memory_order_relaxed

static std::atomic_int *x;

static void a(void *obj)
{
	int idx = *((int *)obj);

	if (idx > 0)
		x[idx - 1].load(load_mo);

	if (idx < N)
		x[idx].store(1, store_mo);
	else
		x[0].load(load_mo);
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
