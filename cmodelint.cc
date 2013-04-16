#include "model.h"
#include "action.h"
#include "cmodelint.h"
#include "threads-model.h"

/** Performs a read action.*/
uint64_t model_read_action(void * obj, memory_order ord) {
	return model->switch_to_master(new ModelAction(ATOMIC_READ, ord, obj));
}

/** Performs a write action.*/
void model_write_action(void * obj, memory_order ord, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, ord, obj, val));
}

/** Performs an init action. */
void model_init_action(void * obj, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_INIT, memory_order_relaxed, obj, val));
}

/**
 * Performs the read part of a RMW action. The next action must either be the
 * write part of the RMW action or an explicit close out of the RMW action w/o
 * a write.
 */
uint64_t model_rmwr_action(void *obj, memory_order ord) {
	return model->switch_to_master(new ModelAction(ATOMIC_RMWR, ord, obj));
}

/** Performs the write part of a RMW action. */
void model_rmw_action(void *obj, memory_order ord, uint64_t val) {
	model->switch_to_master(new ModelAction(ATOMIC_RMW, ord, obj, val));
}

/** Closes out a RMW action without doing a write. */
void model_rmwc_action(void *obj, memory_order ord) {
	model->switch_to_master(new ModelAction(ATOMIC_RMWC, ord, obj));
}

/** Issues a fence operation. */
void model_fence_action(memory_order ord) {
	model->switch_to_master(new ModelAction(ATOMIC_FENCE, ord, FENCE_LOCATION));
}
