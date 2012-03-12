#include "model.h"
#include "schedule.h"
#include <stdlib.h>
#include <string.h>

struct model_checker *model;

void model_checker_init(void)
{
	model = malloc(sizeof(*model));
	memset(model, 0, sizeof(*model));
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
