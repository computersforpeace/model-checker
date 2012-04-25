#ifndef __LIBRACE_H__
#define __LIBRACE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

	void store_8(void *addr, uint8_t val);
	void store_16(void *addr, uint16_t val);
	void store_32(void *addr, uint32_t val);
	void store_64(void *addr, uint64_t val);

	uint8_t load_8(void *addr);
	uint16_t load_16(void *addr);
	uint32_t load_32(void *addr);
	uint64_t load_64(void *addr);

#ifdef __cplusplus
}
#endif

#endif /* __LIBRACE_H__ */
