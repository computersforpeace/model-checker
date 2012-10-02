#include <algorithm>
#include <cstring>
#include <stdlib.h>

#include "model.h"
#include "action.h"
#include "clockvector.h"
#include "common.h"

/**
 * Constructs a new ClockVector, given a parent ClockVector and a first
 * ModelAction. This constructor can assign appropriate default settings if no
 * parent and/or action is supplied.
 * @param parent is the previous ClockVector to inherit (i.e., clock from the
 * same thread or the parent that created this thread)
 * @param act is an action with which to update the ClockVector
 */
ClockVector::ClockVector(ClockVector *parent, ModelAction *act)
{
	num_threads = model->get_num_threads();
	clock = (modelclock_t *)snapshot_calloc(num_threads, sizeof(int));
	if (parent)
		std::memcpy(clock, parent->clock, parent->num_threads * sizeof(modelclock_t));

	if (act)
		clock[id_to_int(act->get_tid())] = act->get_seq_number();
}

/** @brief Destructor */
ClockVector::~ClockVector()
{
	snapshot_free(clock);
}

/**
 * Merge a clock vector into this vector, using a pairwise comparison. The
 * resulting vector length will be the maximum length of the two being merged.
 * @param cv is the ClockVector being merged into this vector.
 */
void ClockVector::merge(const ClockVector *cv)
{
	modelclock_t *clk = clock;
	bool resize = false;

	ASSERT(cv != NULL);

	if (cv->num_threads > num_threads) {
		resize = true;
		clk = (modelclock_t *)snapshot_malloc(cv->num_threads * sizeof(modelclock_t));
	}

	/* Element-wise maximum */
	for (int i = 0; i < num_threads; i++)
		clk[i] = std::max(clock[i], cv->clock[i]);

	if (resize) {
		for (int i = num_threads; i < cv->num_threads; i++)
			clk[i] = cv->clock[i];
		num_threads = cv->num_threads;
		snapshot_free(clock);
	}
	clock = clk;
}

/**
 * Check whether this vector's thread has synchronized with another action's
 * thread. This effectively checks the happens-before relation (or actually,
 * happens after), but it's easier to compare two ModelAction events directly,
 * using ModelAction::happens_before.
 *
 * @see ModelAction::happens_before
 *
 * @return true if this ClockVector's thread has synchronized with act's
 * thread, false otherwise. That is, this function returns:
 * <BR><CODE>act <= cv[act->tid]</CODE>
 */
bool ClockVector::synchronized_since(const ModelAction *act) const
{
	int i = id_to_int(act->get_tid());

	if (i < num_threads)
		return act->get_seq_number() <= clock[i];
	return false;
}

bool ClockVector::has_synchronized_with(const ClockVector *cv) const
{
	ASSERT(cv);
	if (cv->num_threads > num_threads)
		return false;
	for (int i = 0; i < cv->num_threads; i++)
		if (cv->clock[i] > clock[i])
			return false;
	return true;
}

/** Gets the clock corresponding to a given thread id from the clock vector. */
modelclock_t ClockVector::getClock(thread_id_t thread) {
	int threadid = id_to_int(thread);

	if (threadid < num_threads)
		return clock[threadid];
	else
		return 0;
}

/** @brief Formats and prints this ClockVector's data. */
void ClockVector::print() const
{
	int i;
	printf("CV: (");
	for (i = 0; i < num_threads; i++)
		printf("%2u%s", clock[i], (i == num_threads - 1) ? ")\n" : ", ");
}
