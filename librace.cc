#include "librace.h"
#include "common.h"

void store_8(void *addr, uint8_t val)
{
	DEBUG("addr = %p, val = %u\n", addr, val);
}

void store_16(void *addr, uint16_t val)
{
	DEBUG("addr = %p, val = %u\n", addr, val);
}

void store_32(void *addr, uint32_t val)
{
	DEBUG("addr = %p, val = %u\n", addr, val);
}

void store_64(void *addr, uint64_t val)
{
	DEBUG("addr = %p, val = %llu\n", addr, val);
}

uint8_t load_8(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return 0;
}

uint16_t load_16(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return 0;
}

uint32_t load_32(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return 0;
}

uint64_t load_64(void *addr)
{
	DEBUG("addr = %p\n", addr);
	return 0;
}
