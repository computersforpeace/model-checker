#ifndef MUTEX_H
#define MUTEX_H
#include "threads.h"

namespace std {
	class mutex {
	public:
		mutex();
		~mutex();
		void lock();
		bool try_lock();
		void unlock();
	private:
		thread_id_t owner;
		bool islocked;
	};
}
#endif
