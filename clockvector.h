/** @file clockvector.h
 *  @brief Implements a clock vector.
 */

#ifndef __CLOCKVECTOR_H__
#define __CLOCKVECTOR_H__

#include "mymemory.h"
#include "modeltypes.h"

/* Forward declaration */
class ModelAction;

class ClockVector {
public:
	ClockVector(ClockVector *parent = NULL, ModelAction *act = NULL);
	~ClockVector();
	bool merge(const ClockVector *cv);
	bool synchronized_since(const ModelAction *act) const;

	void print() const;
	modelclock_t getClock(thread_id_t thread);

	SNAPSHOTALLOC
private:
	/** @brief Holds the actual clock data, as an array. */
	modelclock_t *clock;

	/** @brief The number of threads recorded in clock (i.e., its length).  */
	int num_threads;
};

#endif /* __CLOCKVECTOR_H__ */
