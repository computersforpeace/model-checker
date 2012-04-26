#ifndef __LIBATOMIC_H__
#define __LIBATOMIC_H__

#ifdef __cplusplus
extern "C" {
#endif

	typedef enum memory_order {
		memory_order_relaxed,
		memory_order_consume,
		memory_order_acquire,
		memory_order_release,
		memory_order_acq_rel,
		memory_order_seq_cst
	} memory_order;

	typedef struct atomic_object {
		int value;
	} atomic_int;

	void atomic_store_explicit(struct atomic_object *obj, int value, memory_order order);
#define atomic_store(A, B) atomic_store_explicit((A), (B), memory_order_seq_cst)

	int atomic_load_explicit(struct atomic_object *obj, memory_order order);
#define atomic_load(A) atomic_load_explicit((A), memory_order_seq_cst)

	void atomic_init(struct atomic_object *obj, int value);

#ifdef __cplusplus
}
#endif

#endif /* __LIBATOMIC_H__ */
