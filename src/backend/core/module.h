#ifndef SMSBARE_BACKEND_CORE_MODULE_H
#define SMSBARE_BACKEND_CORE_MODULE_H

#include <circle/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BACKEND_MODULE_ABI_V1 1u

typedef struct {
    boolean initialized;
    boolean running;
    unsigned long frames;
    int last_pc;
    int last_error;
} BackendRuntimeStatus;

typedef struct {
    unsigned logical_width;
    unsigned logical_height;
    unsigned pitch_pixels;
    unsigned source_x;
    unsigned source_y;
    unsigned source_width;
    unsigned source_height;
    boolean hires_mode;
    boolean interlaced_mode;
    boolean woven_hires;
    boolean field_parity_odd;
    boolean disable_scanline_postfx;
} BackendVideoPresentation;

typedef struct {
    unsigned abi_version;
    void (*uart_write)(const char *text);
    unsigned long long (*monotonic_time_us)(void);
} BackendHostExportsV1;

typedef struct BackendOps {
    boolean (*init)(const BackendHostExportsV1 *host, int *out_error_code);
    boolean (*reset)(void);
    boolean (*step_frame)(void);
    void (*shutdown)(void);
    unsigned (*video_frame)(const u16 **pixels, unsigned *width, unsigned *height);
    void (*get_video_presentation)(BackendVideoPresentation *presentation);
    unsigned (*audio_pull)(int16_t *dst, unsigned max_samples);
    void (*input_push_joystick)(u8 bridge_bits);
    void (*input_push_keyboard)(u8 modifiers, const unsigned char raw_keys[6]);
    boolean (*media_load_cartridge)(unsigned slot_index, const char *path);
    boolean (*media_load_disk)(unsigned drive_index, const char *path);
    boolean (*media_load_cassette)(const char *path);
    BackendRuntimeStatus (*get_status)(void);
} BackendOps;

typedef struct BackendModuleV1 {
    unsigned abi_version;
    const char *family_id;
    const char *backend_id;
    const char *display_name;
    unsigned capability_bits;
    const BackendOps *ops;
    const void *family_ops;
} BackendModuleV1;

typedef struct {
    const char *backend_family;
    const char *requested_backend_id;
    const char *selected_backend_id;
    const char *active_backend_name;
    boolean requested_backend_found;
    boolean used_default_backend;
    boolean init_attempted;
    boolean available;
    int init_error_code;
} BackendInfo;

#ifdef __cplusplus
}
#endif

#endif
