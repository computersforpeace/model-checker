#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <list>

#include "libthreads.h"
#include "model.h"

class Scheduler {
public:
	virtual void add_thread(struct thread *t) = 0;
	virtual struct thread * next_thread(void) = 0;
	virtual struct thread * get_current_thread(void) = 0;
};

class DefaultScheduler: public Scheduler {
public:
	void add_thread(struct thread *t);
	struct thread * next_thread(void);
	struct thread * get_current_thread(void);
private:
	std::list<struct thread *> queue;
	struct thread *current;
};

#endif /* __SCHEDULE_H__ */
