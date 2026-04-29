#ifndef SMSBARE_PLATFORM_H
#define SMSBARE_PLATFORM_H

#include <stdint.h>

uint64_t platform_get_time_us(void);
void platform_delay_us(uint32_t us);
void platform_delay_ms(uint32_t ms);

#endif