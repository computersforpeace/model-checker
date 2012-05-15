#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "librace.h"
#include "common.h"

void store_8(void *addr, uint8_t val)
{
	DEBUG("addr = %p, val = %" PRIu8 "\n", addr, val);
	(*(uint8_t *)addr) = val;
}

void store_16(void *addr, uint16_t val)
{
	DEBUG("addr = %p, val = %" PRIu16 "\n", addr, val);
	(*(uint16_t *)addr) = val;
}

void store_32(void *addr, uint32_t val)
{
	DEBUG("addr = %p, val = %" PRIu32 "\n", addr, val);
	(*(uint32_t *)addr) = val;
}

void store_64(void *addr, uint64_t val)
{
	DEBUG("addr = %p, val = %" PRIu64 "\n", addr, val);
	(*(uint64_t *)addr) = val;
}

uint8_t load_8(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return *((uint8_t *)addr);
}

uint16_t load_16(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return *((uint16_t *)addr);
}

uint32_t load_32(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return *((uint32_t *)addr);
}

uint64_t load_64(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return *((uint64_t *)addr);
}
