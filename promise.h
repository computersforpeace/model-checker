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
	thread_id_t tid;
};

class Promise {
 public:
	Promise(ModelAction *read, struct future_value fv) :
		num_available_threads(0),
		value(fv.value),
		expiration(fv.expiration),
		read(read),
		write(NULL)
	{
		add_thread(fv.tid);
		eliminate_thread(read->get_tid());
	}
	modelclock_t get_expiration() const { return expiration; }
	ModelAction * get_action() const { return read; }
	bool eliminate_thread(thread_id_t tid);
	void add_thread(thread_id_t tid);
	bool thread_is_available(thread_id_t tid) const;
	bool has_failed() const;
	uint64_t get_value() const { return value; }
	void set_write(const ModelAction *act) { write = act; }
	const ModelAction * get_write() { return write; }
	int get_num_available_threads() { return num_available_threads; }

	void print() const;

	SNAPSHOTALLOC
 private:
	/** @brief Thread ID(s) for thread(s) that potentially can satisfy this
	 *  promise */
	std::vector< bool, SnapshotAlloc<bool> > available_thread;

	int num_available_threads;

	const uint64_t value;
	const modelclock_t expiration;

	/** @brief The action which reads a promised value */
	ModelAction * const read;

	const ModelAction *write;
};

#endif
