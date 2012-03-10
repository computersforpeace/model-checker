#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include "libthreads.h"

void schedule_add_thread(struct thread *t);
struct thread *schedule_choose_next(void);

#endif /* __SCHEDULE_H__ */
