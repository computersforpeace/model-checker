#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <queue>

#include "threads_internal.h"
#include "model.h"

class Scheduler {
public:
	void add_thread(Thread *t);
	Thread * next_thread(void);
	Thread * get_current_thread(void);
private:
	std::queue<Thread *> queue;
	Thread *current;
};

#endif /* __SCHEDULE_H__ */
