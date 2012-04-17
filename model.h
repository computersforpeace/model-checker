#ifndef __MODEL_H__
#define __MODEL_H__

#include <list>
#include <map>

#include "schedule.h"
#include "libthreads.h"
#include "libatomic.h"
#include "threads.h"

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
	Thread *system_thread;

	void add_system_thread(Thread *t);

	void set_current_action(ModelAction *act) { current_action = act; }
	void check_current_action(void);
	void print_trace(void);

	int add_thread(Thread *t);
	Thread *get_thread(thread_id_t tid) { return thread_map[tid]; }

	void assign_id(Thread *t);

	int switch_to_master(ModelAction *act);
private:
	int used_thread_id;
	class ModelAction *current_action;
	std::list<class ModelAction *> action_trace;
	std::map<thread_id_t, class Thread *> thread_map;
};

extern ModelChecker *model;

int thread_switch_to_master(ModelAction *act);

#endif /* __MODEL_H__ */
