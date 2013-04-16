#include <mutex>

#include "model.h"
#include "execution.h"
#include "threads-model.h"
#include "clockvector.h"
#include "action.h"

namespace std {

mutex::mutex()
{
	state.locked = NULL;
	thread_id_t tid = thread_current()->get_id();
	state.alloc_tid = tid;
	state.alloc_clock = model->get_execution()->get_cv(tid)->getClock(tid);
}
	
void mutex::lock()
{
	model->switch_to_master(new ModelAction(ATOMIC_LOCK, std::memory_order_seq_cst, this));
}
	
bool mutex::try_lock()
{
	return model->switch_to_master(new ModelAction(ATOMIC_TRYLOCK, std::memory_order_seq_cst, this));
}

void mutex::unlock()
{
	model->switch_to_master(new ModelAction(ATOMIC_UNLOCK, std::memory_order_seq_cst, this));
}

}
