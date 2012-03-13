#include "model.h"
#include "schedule.h"
#include <stdlib.h>
#include <string.h>

struct model_checker *model;

void model_checker_add_system_thread(struct thread *t)
{
	model->system_thread = t;
}

void model_checker_init(void)
{
	model = malloc(sizeof(*model));
	memset(model, 0, sizeof(*model));

	/* First thread created (system_thread) will have id 1 */
	model->used_thread_id = 0;

	scheduler_init(model);
}

void model_checker_exit(void)
{
	struct scheduler *sched = model->scheduler;

	if (sched->exit)
		sched->exit();
	free(sched);
	free(model);
}

void model_checker_assign_id(struct thread *t)
{
	t->id = ++model->used_thread_id;
}
