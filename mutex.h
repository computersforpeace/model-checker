#ifndef MUTEX_H
#define MUTEX_H
#include "threads.h"

namespace std {
	struct mutex_state {
		bool islocked;
	};

	class mutex {
	public:
		mutex();
		~mutex();
		void lock();
		bool try_lock();
		void unlock();
		struct mutex_state * get_state() {return &state;}
		
	private:
		struct mutex_state state;
	};
}
#endif
