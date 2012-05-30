#include <algorithm>
#include <cstring>
#include <stdlib.h>

#include "model.h"
#include "action.h"
#include "clockvector.h"
#include "common.h"

ClockVector::ClockVector(ClockVector *parent, ModelAction *act)
{
	num_threads = model->get_num_threads();
	clock = (int *)MYMALLOC(num_threads * sizeof(int));
	memset(clock, 0, num_threads * sizeof(int));
	if (parent)
		std::memcpy(clock, parent->clock, parent->num_threads * sizeof(int));

	if (act)
		clock[id_to_int(act->get_tid())] = act->get_seq_number();
}

ClockVector::~ClockVector()
{
	MYFREE(clock);
}

void ClockVector::merge(ClockVector *cv)
{
	int *clk = clock;
	bool resize = false;

	ASSERT(cv != NULL);

	if (cv->num_threads > num_threads) {
		resize = true;
		clk = (int *)MYMALLOC(cv->num_threads * sizeof(int));
	}

	/* Element-wise maximum */
	for (int i = 0; i < num_threads; i++)
		clk[i] = std::max(clock[i], cv->clock[i]);

	if (resize) {
		for (int i = num_threads; i < cv->num_threads; i++)
			clk[i] = cv->clock[i];
		num_threads = cv->num_threads;
		MYFREE(clock);
	}
	clock = clk;
}

bool ClockVector::happens_before(ModelAction *act, thread_id_t id)
{
	int i = id_to_int(id);

	if (i < num_threads)
		return act->get_seq_number() < clock[i];
	return false;
}

void ClockVector::print()
{
	int i;
	printf("CV: (");
	for (i = 0; i < num_threads; i++)
		printf("%2d%s", clock[i], (i == num_threads - 1) ? ")\n" : ", ");
}
