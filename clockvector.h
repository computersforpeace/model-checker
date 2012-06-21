/** @file clockvector.h
 *  @brief Implements a clock vector.
 */

#ifndef __CLOCKVECTOR_H__
#define __CLOCKVECTOR_H__

#include "threads.h"
#include "mymemory.h"

/* Forward declaration */
class ModelAction;

class ClockVector {
public:
	ClockVector(ClockVector *parent = NULL, ModelAction *act = NULL);
	~ClockVector();
	void merge(ClockVector *cv);
	bool synchronized_since(ModelAction *act);

	void print();

	MEMALLOC
private:
	int *clock;
	int num_threads;
};

#endif /* __CLOCKVECTOR_H__ */
