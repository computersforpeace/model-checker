/** @file promise.h
 *
 *  @brief Promise class --- tracks future obligations for execution
 *  related to weakly ordered writes.
 */

#ifndef __PROMISE_H__
#define __PROMISE_H__
#include <inttypes.h>


class ModelAction;

class Promise {
 public:
	Promise(ModelAction * act, uint64_t value);
	const ModelAction * get_action() { return read; }
	int increment_threads() { return ++numthreads; }
	
 private:
	uint64_t value;
	ModelAction *read;
	unsigned int numthreads;
};

#endif
