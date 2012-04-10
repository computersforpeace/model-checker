#ifndef __MODEL_H__
#define __MODEL_H__

#include "schedule.h"
#include "libthreads.h"
#include "libatomic.h"

#define VALUE_NONE -1

typedef enum action_type {
	THREAD_CREATE,
	THREAD_YIELD,
	THREAD_JOIN,
	ATOMIC_READ,
	ATOMIC_WRITE
} action_type_t;

class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, int value);
	void print(void);
private:
	action_type type;
	memory_order order;
	void *location;
	thread_id_t tid;
	int value;
};

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
