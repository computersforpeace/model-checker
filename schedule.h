#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <queue>

#include "libthreads.h"
#include "model.h"

class Scheduler {
public:
	void add_thread(struct thread *t);
	struct thread * next_thread(void);
	struct thread * get_current_thread(void);
private:
	std::queue<struct thread *> queue;
	struct thread *current;
};

#endif /* __SCHEDULE_H__ */
