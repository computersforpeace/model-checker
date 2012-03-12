#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include "libthreads.h"
#include "model.h"

struct scheduler {
	void (*init)(void);
	void (*exit)(void);
	void (*add_thread)(struct thread *t);
	struct thread * (*next_thread)(void);
	struct thread * (*get_current_thread)(void);

	void *priv;
};

void scheduler_init(struct model_checker *mod);

#endif /* __SCHEDULE_H__ */
