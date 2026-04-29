#ifndef SMSBARE_COMMON_H
#define SMSBARE_COMMON_H

#include <stdint.h>
#include <stddef.h>

#define MMIO_BASE 0x3F000000UL

static inline void mmio_write(uintptr_t reg, uint32_t value)
{
    *(volatile uint32_t *)reg = value;
}

static inline uint32_t mmio_read(uintptr_t reg)
{
    return *(volatile uint32_t *)reg;
}

static inline void barrier_data_sync(void)
{
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline void barrier_inst_sync(void)
{
    __asm__ volatile("isb" ::: "memory");
}

#endif