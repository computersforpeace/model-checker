#include "mutex.h"
#include "model.h"


namespace std {
mutex::mutex() :
	owner(0), islocked(false)
{

}
	
void mutex::lock() {
  model->switch_to_master(new ModelAction(ATOMIC_LOCK, std::memory_order_seq_cst, this));
}
	
bool mutex::try_lock() {
  model->switch_to_master(new ModelAction(ATOMIC_TRYLOCK, std::memory_order_seq_cst, this));
  return thread_current()->get_return_value();
}

void mutex::unlock() {
  model->switch_to_master(new ModelAction(ATOMIC_UNLOCK, std::memory_order_seq_cst, this));
}

}
