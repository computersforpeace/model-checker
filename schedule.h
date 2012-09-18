/** @file schedule.h
 *	@brief Thread scheduler.
 */

#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <list>
#include "mymemory.h"

/* Forward declaration */
class Thread;

/** @brief The Scheduler class performs the mechanics of Thread execution
 * scheduling. */
class Scheduler {
public:
	Scheduler();
	void add_thread(Thread *t);
	void remove_thread(Thread *t);
	void sleep(Thread *t);
	void wake(Thread *t);
	Thread * next_thread(Thread *t);
	Thread * get_current_thread() const;
	void print() const;
	bool * get_enabled() { return is_enabled; };

	SNAPSHOTALLOC
private:
	/** The list of available Threads that are not currently running */
	bool * is_enabled;
	int enabled_len;
	int curr_thread_index;
	void set_enabled(Thread *t, bool enabled_status);

	/** The currently-running Thread */
	Thread *current;
};

#endif /* __SCHEDULE_H__ */
