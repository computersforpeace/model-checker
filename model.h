#ifndef __MODEL_H__
#define __MODEL_H__

struct model_checker {
	struct scheduler *scheduler;
	struct thread *system_thread;

	/* "Private" fields */
	int used_thread_id;
};

extern struct model_checker *model;

void model_checker_init(void);
void model_checker_add_system_thread(struct thread *t);
void model_checker_assign_id(struct thread *t);

#endif /* __MODEL_H__ */
