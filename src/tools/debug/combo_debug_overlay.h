#ifndef SMSBARE_CIRCLE_SMSBARE_DEBUG_OVERLAY_H
#define SMSBARE_CIRCLE_SMSBARE_DEBUG_OVERLAY_H

#include <circle/string.h>
#include <circle/types.h>

typedef struct
{
    unsigned tick;
    unsigned mmu_enabled;
    unsigned dcache_enabled;
    unsigned icache_enabled;
    unsigned paused;

    unsigned queue_avail;
    unsigned queue_free;
    unsigned keyboard_attached;
    unsigned sound_active;
    unsigned sound_start_attempts;
    unsigned sound_start_failures;
    unsigned perf_tick;

    unsigned audio_write_ops;
    unsigned audio_drop_ops;
    unsigned samples_written_mod;
    unsigned samples_pulled_mod;
    unsigned source_peak;

    unsigned audio_peak;
    unsigned clip_max;
    unsigned audio_gain_ui;
    unsigned fps_x10;
    unsigned last_hid;
    unsigned last_runtime;

    unsigned joy_bridge;
    unsigned joy_mapped;
    unsigned backend_ready;
    int backend_last_error;
    unsigned initramfs_files;

    unsigned long backend_frames;
    unsigned backend_last_pc;
    unsigned backend_pc_repeat_count;
    unsigned ring_level;
    unsigned pacing_err_avg;
    unsigned pacing_err_max;

    unsigned save_stage;
    unsigned save_code;
    unsigned save_aux;
    unsigned load_diag;
    unsigned boot_to_backend_ms;

    unsigned mapper_a;
    unsigned mapper_b;
    unsigned cart_a_loaded;
    unsigned cart_b_loaded;
    unsigned active_boot_mode;
    unsigned active_model_profile;

    unsigned fm_music_enabled;
    unsigned fm_layer_gain_pct;
    unsigned fm_mix_enabled;
    unsigned scc_cart_enabled;
    unsigned scc_mix_enabled;
    unsigned audio_gain_effective;
    unsigned audio_muted;
    unsigned psg_mixer;
    unsigned master_switch;

    unsigned layer1_muted;
    unsigned layer2_muted;
    unsigned layer3_muted;
    unsigned psg_mix_enabled;
} TComboDebugOverlayData;

class CComboDebugOverlay
{
public:
    typedef void (*TWriteFn)(const char *text, void *context);

    CComboDebugOverlay(void);
    void SetWriter(TWriteFn write_fn, void *context);
    void Clear(void);
    void Render(const TComboDebugOverlayData &d);

private:
    void Write(const char *text);

private:
    TWriteFn m_WriteFn;
    void *m_Context;
};

#endif
