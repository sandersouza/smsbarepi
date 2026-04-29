#ifndef SMSBARE_CIRCLE_AUDIO_RUNTIME_H
#define SMSBARE_CIRCLE_AUDIO_RUNTIME_H

#include <stdint.h>

typedef struct circle_audio_snapshot {
    uint32_t ready;
    uint32_t active;
    uint32_t queue_frames;
    uint32_t queue_avail;
    uint32_t writes;
    uint32_t drops;
    uint32_t last_level;
    uint32_t writable_hits;
    uint32_t writable_misses;
} circle_audio_snapshot_t;

#ifdef __cplusplus
extern "C" {
#endif

int circle_audio_runtime_init(void);
void circle_audio_runtime_poll(circle_audio_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif