#include <stdio.h>

#include "threads.h"
#include "librace.h"
#include "stdatomic.h"
#include <mutex>
#include <condition_variable>

std::mutex * m;
std::condition_variable *v;
int shareddata;

static void a(void *obj)
{
	
	m->lock();
	while(load_32(&shareddata)==0)
		v->wait(*m);
	m->unlock();

}

static void b(void *obj)
{
	m->lock();
	store_32(&shareddata, (unsigned int) 1);
	v->notify_all();
	m->unlock();
}

int user_main(int argc, char **argv)
{
	thrd_t t1, t2;
	store_32(&shareddata, (unsigned int) 0);
	m=new std::mutex();
	v=new std::condition_variable();

	thrd_create(&t1, (thrd_start_t)&a, NULL);
	thrd_create(&t2, (thrd_start_t)&b, NULL);

	thrd_join(t1);
	thrd_join(t2);
	return 0;
}
