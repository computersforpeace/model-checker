#include "impatomic.h"
#include "common.h"
#include "model.h"
#include "threads.h"

namespace std {

bool atomic_flag_test_and_set_explicit ( volatile atomic_flag * __a__, memory_order __x__ ) {
	volatile bool * __p__ = &((__a__)->__f__);
	model->switch_to_master(new ModelAction(ATOMIC_RMWR, __x__, (void *) __p__));
	bool result = (bool) thread_current()->get_return_value();
	model->switch_to_master(new ModelAction(ATOMIC_RMW, __x__, (void *) __p__, true));
	return result;
}

bool atomic_flag_test_and_set( volatile atomic_flag* __a__ )
{ return atomic_flag_test_and_set_explicit( __a__, memory_order_seq_cst ); }

void atomic_flag_clear_explicit
( volatile atomic_flag* __a__, memory_order __x__ )
{
	volatile bool * __p__ = &((__a__)->__f__);
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, __x__, (void *) __p__, false));
}

void atomic_flag_clear( volatile atomic_flag* __a__ )
{ atomic_flag_clear_explicit( __a__, memory_order_seq_cst ); }

void atomic_flag_fence( const volatile atomic_flag* __a__, memory_order __x__ )
{
	ASSERT(0);
}

void __atomic_flag_wait__( volatile atomic_flag* __a__ )
{ while ( atomic_flag_test_and_set( __a__ ) ); }

void __atomic_flag_wait_explicit__( volatile atomic_flag* __a__,
                                    memory_order __x__ )
{ while ( atomic_flag_test_and_set_explicit( __a__, __x__ ) ); }

}
