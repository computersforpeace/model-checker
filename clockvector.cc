#include <cstring>
#include <stdlib.h>

#include "action.h"
#include "clockvector.h"
#include "common.h"
#include "threads-model.h"

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
	ASSERT(act);
	num_threads = int_to_id(act->get_tid()) + 1;
	if (parent && parent->num_threads > num_threads)
		num_threads = parent->num_threads;

	clock = (modelclock_t *)snapshot_calloc(num_threads, sizeof(int));
	if (parent)
		std::memcpy(clock, parent->clock, parent->num_threads * sizeof(modelclock_t));

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
bool ClockVector::merge(const ClockVector *cv)
{
	ASSERT(cv != NULL);
	bool changed = false;
	if (cv->num_threads > num_threads) {
		clock = (modelclock_t *)snapshot_realloc(clock, cv->num_threads * sizeof(modelclock_t));
		for (int i = num_threads; i < cv->num_threads; i++)
			clock[i] = 0;
		num_threads = cv->num_threads;
	}

	/* Element-wise maximum */
	for (int i = 0; i < cv->num_threads; i++)
		if (cv->clock[i] > clock[i]) {
			clock[i] = cv->clock[i];
			changed = true;
		}
	
	return changed;
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
	model_print("(");
	for (i = 0; i < num_threads; i++)
		model_print("%2u%s", clock[i], (i == num_threads - 1) ? ")\n" : ", ");
}
