/** @file cmodelint.h
 *  @brief C interface to the model checker.
 */

#ifndef CMODELINT_H
#define CMODELINT_H
#include <inttypes.h>
#include "memoryorder.h"

#if __cplusplus
using std::memory_order;
extern "C" {
#endif

uint64_t model_read_action(void * obj, memory_order ord);
void model_write_action(void * obj, memory_order ord, uint64_t val);
void model_init_action(void * obj, uint64_t val);
uint64_t model_rmwr_action(void *obj, memory_order ord);
void model_rmw_action(void *obj, memory_order ord, uint64_t val);
void model_rmwc_action(void *obj, memory_order ord);
void model_fence_action(memory_order ord);


#if __cplusplus
}
#endif

#endif
