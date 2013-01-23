/** @file promise.h
 *
 *  @brief Promise class --- tracks future obligations for execution
 *  related to weakly ordered writes.
 */

#ifndef __PROMISE_H__
#define __PROMISE_H__

#include <inttypes.h>
#include "threads-model.h"

#include "model.h"
#include "modeltypes.h"

struct future_value {
	uint64_t value;
	modelclock_t expiration;
};

class Promise {
 public:
	Promise(ModelAction *act, struct future_value fv) :
		value(fv.value),
		expiration(fv.expiration),
		read(act),
		write(NULL)
	{
		eliminate_thread(act->get_tid());
	}
	modelclock_t get_expiration() const { return expiration; }
	ModelAction * get_action() const { return read; }
	bool eliminate_thread(thread_id_t tid);
	bool thread_is_eliminated(thread_id_t tid) const;
	bool has_failed() const;
	uint64_t get_value() const { return value; }
	void set_write(const ModelAction *act) { write = act; }
	const ModelAction * get_write() { return write; }

	SNAPSHOTALLOC
 private:
	std::vector< bool, SnapshotAlloc<bool> > eliminated_thread;
	const uint64_t value;
	const modelclock_t expiration;
	ModelAction * const read;
	const ModelAction *write;
};

#endif
