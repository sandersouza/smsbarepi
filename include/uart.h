#ifndef SMSBARE_UART_H
#define SMSBARE_UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *text);
void uart_put_hex32(uint32_t value);

#endif