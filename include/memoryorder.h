#ifndef MEMORYORDER_H
#define MEMORYORDER_H
#ifdef __cplusplus
#include <cstddef>
namespace std {
#else
#include <stddef.h>
#include <stdbool.h>
#endif


typedef enum memory_order {
    memory_order_relaxed, memory_order_acquire, memory_order_release,
    memory_order_acq_rel, memory_order_seq_cst
} memory_order;


#ifdef __cplusplus
}
#endif



#endif
