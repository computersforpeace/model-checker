/* -*- Mode: C; indent-tabs-mode: t -*- */

#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <list>
#include "mymemory.h"

/* Forward declaration */
class Thread;

class Scheduler {
public:
	Scheduler();
	void add_thread(Thread *t);
	void remove_thread(Thread *t);
	Thread * next_thread(void);
	Thread * get_current_thread(void);
	void print();
  MEMALLOC
private:
	std::list<Thread *, MyAlloc< Thread * > > readyList;
	Thread *current;
};

#endif /* __SCHEDULE_H__ */
