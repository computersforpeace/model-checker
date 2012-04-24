#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <list>

#include "threads.h"
#include "model.h"

class Scheduler {
public:
	void add_thread(Thread *t);
	void remove_thread(Thread *t);
	Thread * next_thread(void);
	Thread * get_current_thread(void);
	void print();
private:
	std::list<Thread *> readyList;
	Thread *current;
};

#endif /* __SCHEDULE_H__ */
