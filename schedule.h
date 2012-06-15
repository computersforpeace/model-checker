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
	Thread * next_thread(void);
	Thread * get_current_thread(void);
	void print();

	SNAPSHOTALLOC
private:
	std::list<Thread *> readyList;
	Thread *current;
};

#endif /* __SCHEDULE_H__ */
