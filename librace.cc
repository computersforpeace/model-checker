#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "librace.h"
#include "common.h"
#include "datarace.h"
#include "model.h"
#include "threads.h"

void store_8(void *addr, uint8_t val)
{
	DEBUG("addr = %p, val = %" PRIu8 "\n", addr, val);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckWrite(tid, addr, cv);
	(*(uint8_t *)addr) = val;
}

void store_16(void *addr, uint16_t val)
{
	DEBUG("addr = %p, val = %" PRIu16 "\n", addr, val);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckWrite(tid, addr, cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+1), cv);
	(*(uint16_t *)addr) = val;
}

void store_32(void *addr, uint32_t val)
{
	DEBUG("addr = %p, val = %" PRIu32 "\n", addr, val);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckWrite(tid, addr, cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+1), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+2), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+3), cv);
	(*(uint32_t *)addr) = val;
}

void store_64(void *addr, uint64_t val)
{
	DEBUG("addr = %p, val = %" PRIu64 "\n", addr, val);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckWrite(tid, addr, cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+1), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+2), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+3), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+4), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+5), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+6), cv);
	raceCheckWrite(tid, (void *)(((uintptr_t)addr)+7), cv);
	(*(uint64_t *)addr) = val;
}

uint8_t load_8(void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckRead(tid, addr, cv);
	return *((uint8_t *)addr);
}

uint16_t load_16(void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckRead(tid, addr, cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+1), cv);
	return *((uint16_t *)addr);
}

uint32_t load_32(void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckRead(tid, addr, cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+1), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+2), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+3), cv);
	return *((uint32_t *)addr);
}

uint64_t load_64(void *addr)
{
	DEBUG("addr = %p\n", addr);
	thread_id_t tid=thread_current()->get_id();
	ClockVector * cv=model->get_cv(tid);
	raceCheckRead(tid, addr, cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+1), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+2), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+3), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+4), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+5), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+6), cv);
	raceCheckRead(tid, (void *)(((uintptr_t)addr)+7), cv);
	return *((uint64_t *)addr);
}
