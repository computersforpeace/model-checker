
#include <stdint.h>
#include "impatomic.h"


#if defined(__GNUC__)
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 0)
#define USE_SYNC
#endif
#endif

bool atomic_flag_test_and_set_explicit
( volatile atomic_flag* __a__, memory_order __x__ )
{
#ifdef USE_SYNC
    if ( __x__ >= memory_order_acq_rel )
        __sync_synchronize();
    return __sync_lock_test_and_set( &(__a__->__f__), 1 );
#else
    bool result = __a__->__f__;
    __a__->__f__ = true;
    return result;
#endif
}

bool atomic_flag_test_and_set( volatile atomic_flag* __a__ )
{ return atomic_flag_test_and_set_explicit( __a__, memory_order_seq_cst ); }

void atomic_flag_clear_explicit
( volatile atomic_flag* __a__, memory_order __x__ )
{
#ifdef USE_SYNC
    __sync_lock_release( &(__a__->__f__) );
    if ( __x__ >= memory_order_acq_rel )
        __sync_synchronize();
#else
    __a__->__f__ = false;
#endif
}

void atomic_flag_clear( volatile atomic_flag* __a__ )
{ atomic_flag_clear_explicit( __a__, memory_order_seq_cst ); }

void atomic_flag_fence( const volatile atomic_flag* __a__, memory_order __x__ )
{
#ifdef USE_SYNC
    __sync_synchronize();
#endif
}

void __atomic_flag_wait__( volatile atomic_flag* __a__ )
{ while ( atomic_flag_test_and_set( __a__ ) ); }

void __atomic_flag_wait_explicit__( volatile atomic_flag* __a__,
                                    memory_order __x__ )
{ while ( atomic_flag_test_and_set_explicit( __a__, __x__ ) ); }

#define LOGSIZE 4

static atomic_flag volatile __atomic_flag_anon_table__[ 1 << LOGSIZE ] =
{
    ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT,
    ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT,
    ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT,
    ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT, ATOMIC_FLAG_INIT,
};

volatile atomic_flag* __atomic_flag_for_address__( const volatile void* __z__ )
{
    uintptr_t __u__ = (uintptr_t)__z__;
    __u__ += (__u__ >> 2) + (__u__ << 4);
    __u__ += (__u__ >> 7) + (__u__ << 5);
    __u__ += (__u__ >> 17) + (__u__ << 13);
    if ( sizeof(uintptr_t) > 4 ) __u__ += (__u__ >> 31);
    __u__ &= ~((~(uintptr_t)0) << LOGSIZE);
    return __atomic_flag_anon_table__ + __u__;
}

