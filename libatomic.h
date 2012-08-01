/** @file libatomic.h
 *  @brief Basic atomic operations to be exposed to user program.
 */

#ifndef __LIBATOMIC_H__
#define __LIBATOMIC_H__

#include "memoryorder.h"

#ifdef __cplusplus
using std::memory_order;
using std::memory_order_relaxed;
using std::memory_order_acquire;
using std::memory_order_release;
using std::memory_order_acq_rel;
using std::memory_order_seq_cst;
extern "C" {
#endif


#ifdef __cplusplus
}
#endif

#endif /* __LIBATOMIC_H__ */
