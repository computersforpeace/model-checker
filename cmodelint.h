#ifndef CMODELINT_H
#define CMODELINT_H
#include <inttypes.h>

#if __cplusplus
using std::memory_order;
extern "C" {
#endif

uint64_t model_read_action(void * obj, memory_order ord);
void model_write_action(void * obj, memory_order ord, uint64_t val);
void model_rmw_action(void *obj, memory_order ord, uint64_t val);
void model_init_action(void * obj, uint64_t val);

#if __cplusplus
}
#endif

#endif
