#ifndef SMSBARE_CIRCLE_INPUT_RUNTIME_H
#define SMSBARE_CIRCLE_INPUT_RUNTIME_H

#include <stdint.h>

typedef struct circle_input_snapshot {
    uint32_t ready;
    uint32_t usb_tree_updates;
    uint32_t key_events;
    uint32_t keyboard_attached;
    uint32_t last_modifiers;
    uint8_t last_raw_keys[6];
} circle_input_snapshot_t;

#ifdef __cplusplus
extern "C" {
#endif

int circle_input_runtime_init(void);
void circle_input_runtime_poll(circle_input_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif