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
 Promise(ModelAction *act, uint64_t value, modelclock_t expiration) :
	value(value), expiration(expiration), read(act), numthreads(1)
	{ }
	modelclock_t get_expiration() const {return expiration;}
	ModelAction * get_action() const { return read; }
	int increment_threads() { return ++numthreads; }
	uint64_t get_value() const { return value; }

 private:
	const uint64_t value;
	const modelclock_t expiration;
	ModelAction * const read;
	unsigned int numthreads;
};

#endif
