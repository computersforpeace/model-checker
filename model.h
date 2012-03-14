#ifndef __MODEL_H__
#define __MODEL_H__

#include "schedule.h"

class ModelChecker {
public:
	ModelChecker();
	~ModelChecker();
	class Scheduler *scheduler;
	struct thread *system_thread;

	void add_system_thread(struct thread *t);
	void assign_id(struct thread *t);

private:
	int used_thread_id;
};

extern ModelChecker *model;

#endif /* __MODEL_H__ */
