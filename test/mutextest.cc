#include <stdio.h>

#include "threads.h"
#include "librace.h"
#include "stdatomic.h"
#include <mutex>
std::mutex * m;
int shareddata;

static void a(void *obj)
{
	int i;
	for(i=0;i<2;i++) {
		if ((i%2)==0) {
			m->lock();
			store_32(&shareddata,(unsigned int)i);
			m->unlock();
		} else {
			while(!m->try_lock())
				thrd_yield();
			store_32(&shareddata,(unsigned int)i);
			m->unlock();
		}
	}
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;
	m=new std::mutex();

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&a, NULL);

	thrd_join(t1);
	thrd_join(t2);
	return 0;
}
