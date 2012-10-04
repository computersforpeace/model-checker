/** @file promise.h
 *
 *  @brief Promise class --- tracks future obligations for execution
 *  related to weakly ordered writes.
 */

#ifndef __PROMISE_H__
#define __PROMISE_H__

#include <inttypes.h>
#include "threads.h"

#include "model.h"

class Promise {
 public:
 Promise(ModelAction *act, uint64_t value, modelclock_t expiration) :
	value(value), expiration(expiration), read(act), write(NULL)
	{ 
		increment_threads(act->get_tid());
	}
	modelclock_t get_expiration() const {return expiration;}
	ModelAction * get_action() const { return read; }
	bool increment_threads(thread_id_t tid);

	bool has_sync_thread(thread_id_t tid) { 
		unsigned int id=id_to_int(tid); 
		if (id>=synced_thread.size()) {
			return false;
		}
		return synced_thread[id];
	}

	uint64_t get_value() const { return value; }
	void set_write(const ModelAction *act) { write = act; }
	const ModelAction * get_write() { return write; }

	SNAPSHOTALLOC
 private:
	std::vector<bool> synced_thread;
	const uint64_t value;
	const modelclock_t expiration;
	ModelAction * const read;
	const ModelAction * write;
};

#endif
