#include "model.h"
#include "cmodelint.h"

uint64_t model_read_action(void * obj, memory_order ord) {
	model->switch_to_master(new ModelAction(ATOMIC_READ, ord, obj));
	return thread_current()->get_return_value();
}

void model_write_action(void * obj, memory_order ord, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, ord, obj, val));
}

void model_init_action(void * obj, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_INIT, memory_order_relaxed, obj, val));
}

void model_rmw_action(void *obj, memory_order ord, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_RMW, ord, obj, val));
}
