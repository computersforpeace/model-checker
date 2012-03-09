#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include "libthreads.h"

void schedule_add_thread(struct thread *t);
int schedule_choose_next(struct thread **t);

#endif /* __SCHEDULE_H__ */
