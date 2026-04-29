#ifndef SMSBARE_INPUT_STUB_H
#define SMSBARE_INPUT_STUB_H

#include <stdint.h>

typedef struct input_stub_state {
    uint32_t polls;
    uint32_t heartbeat;
} input_stub_state_t;

void input_stub_init(input_stub_state_t *state);
void input_stub_poll(input_stub_state_t *state);

#endif