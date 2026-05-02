#include "../../backend.h"
#include "../../../../../third_party/smsplus/shared.h"

void (*cpu_writemem16)(int address, int data);
void (*cpu_writeport16)(uint16 port, uint8 data);
uint8 (*cpu_readport16)(uint16 port);

enum {
    SmsBareAudioSampleRate = 48000,
    SmsBarePsgClock = 3579545,
    SmsBareFmClock = 3579545,
    SmsBareAudioBridgeGain = 16,
    SmsBareAudioRingSamples = 8192
};

static volatile unsigned g_smsbare_audio_frame_seq = 0u;
static int16_t g_smsbare_audio_ring[SmsBareAudioRingSamples];
static unsigned g_smsbare_audio_rpos = 0u;
static unsigned g_smsbare_audio_wpos = 0u;
static unsigned g_smsbare_audio_level = 0u;
static unsigned g_smsbare_audio_ring_peak = 0u;
static unsigned g_smsbare_audio_drop_count = 0u;
static unsigned g_smsbare_audio_overwrite_samples = 0u;
static unsigned g_smsbare_audio_short_pulls = 0u;
static unsigned g_smsbare_audio_underrun_samples = 0u;
static unsigned g_smsbare_audio_samples_written = 0u;
static unsigned g_smsbare_audio_samples_pulled = 0u;
static unsigned g_smsbare_audio_write_calls = 0u;
static unsigned g_smsbare_audio_last_pull_requested = 0u;
static unsigned g_smsbare_audio_last_pull_returned = 0u;
static unsigned g_smsbare_audio_last_level_before_pull = 0u;
static unsigned g_smsbare_audio_last_level_after_pull = 0u;
static unsigned g_smsbare_audio_last_render_batch = 0u;
static unsigned g_smsbare_audio_max_render_batch = 0u;
static unsigned g_smsbare_audio_source_peak = 0u;
static unsigned g_smsbare_audio_source_nonzero = 0u;

void error_init(void) {}
void error_shutdown(void) {}
void error(char *format, ...)
{
    (void) format;
}

int __wrap_fmunit_detect_r(void)
{
    if (!sms_backend_get_fm_music_enabled() || !sms.use_fm)
    {
        return 0xFF;
    }
    return sms.fm_detect & 0x01;
}

void __wrap_fmunit_detect_w(int data)
{
    sms.fm_detect = (uint8) (data & 0x01);
}

extern uint8 __real_smsj_port_r(uint16 port);

uint8 __wrap_smsj_port_r(uint16 port)
{
    if (((port & 0xFF) == 0xF2) && ((sms.memctrl & 4) != 0))
    {
        return (uint8) __wrap_fmunit_detect_r();
    }
    return __real_smsj_port_r(port);
}

static unsigned smsbare_abs_i32(int value)
{
    return value < 0 ? (unsigned) (-value) : (unsigned) value;
}

static int smsbare_clip16(int value)
{
    if (value > 32767)
    {
        return 32767;
    }
    if (value < -32768)
    {
        return -32768;
    }
    return value;
}

static void smsbare_audio_reset_ring(void)
{
    memset(g_smsbare_audio_ring, 0, sizeof(g_smsbare_audio_ring));
    g_smsbare_audio_rpos = 0u;
    g_smsbare_audio_wpos = 0u;
    g_smsbare_audio_level = 0u;
    g_smsbare_audio_ring_peak = 0u;
    g_smsbare_audio_drop_count = 0u;
    g_smsbare_audio_overwrite_samples = 0u;
    g_smsbare_audio_short_pulls = 0u;
    g_smsbare_audio_underrun_samples = 0u;
    g_smsbare_audio_samples_written = 0u;
    g_smsbare_audio_samples_pulled = 0u;
    g_smsbare_audio_write_calls = 0u;
    g_smsbare_audio_last_pull_requested = 0u;
    g_smsbare_audio_last_pull_returned = 0u;
    g_smsbare_audio_last_level_before_pull = 0u;
    g_smsbare_audio_last_level_after_pull = 0u;
    g_smsbare_audio_last_render_batch = 0u;
    g_smsbare_audio_max_render_batch = 0u;
    g_smsbare_audio_source_peak = 0u;
    g_smsbare_audio_source_nonzero = 0u;
}

static void smsbare_audio_ring_push(int16 sample)
{
    if (g_smsbare_audio_level >= SmsBareAudioRingSamples)
    {
        g_smsbare_audio_rpos = (g_smsbare_audio_rpos + 1u) % SmsBareAudioRingSamples;
        --g_smsbare_audio_level;
        ++g_smsbare_audio_drop_count;
        ++g_smsbare_audio_overwrite_samples;
    }

    g_smsbare_audio_ring[g_smsbare_audio_wpos] = sample;
    g_smsbare_audio_wpos = (g_smsbare_audio_wpos + 1u) % SmsBareAudioRingSamples;
    ++g_smsbare_audio_level;
    if (g_smsbare_audio_level > g_smsbare_audio_ring_peak)
    {
        g_smsbare_audio_ring_peak = g_smsbare_audio_level;
    }
}

static void smsbare_audio_track_peak(unsigned *peak, int value)
{
    const unsigned abs_value = smsbare_abs_i32(value);
    if (abs_value > *peak)
    {
        *peak = abs_value;
    }
}

static void smsplus_bare_mixer_callback(int16 **stream, int16 **output, int length)
{
    const boolean fm_enabled = sms_backend_get_fm_music_enabled();
    unsigned source_peak = 0u;
    unsigned source_nonzero = 0u;

    if (stream == 0 || output == 0 || output[0] == 0 || output[1] == 0 || length <= 0)
    {
        return;
    }

    for (int i = 0; i < length; ++i)
    {
        const int psg_l = stream[STREAM_PSG_L] != 0 ? (int) stream[STREAM_PSG_L][i] : 0;
        const int psg_r = stream[STREAM_PSG_R] != 0 ? (int) stream[STREAM_PSG_R][i] : 0;
        const int fm_mo = (fm_enabled && stream[STREAM_FM_MO] != 0) ? (int) stream[STREAM_FM_MO][i] : 0;
        const int fm_ro = (fm_enabled && stream[STREAM_FM_RO] != 0) ? (int) stream[STREAM_FM_RO][i] : 0;
        const int fm = (fm_mo + fm_ro) / 2;
        const int mixed_l = (fm + psg_l) * SmsBareAudioBridgeGain;
        const int mixed_r = (fm + psg_r) * SmsBareAudioBridgeGain;
        const int clipped_l = smsbare_clip16(mixed_l);
        const int clipped_r = smsbare_clip16(mixed_r);
        const int mono = (clipped_l + clipped_r) / 2;

        smsbare_audio_track_peak(&source_peak, mono);
        if (mono != 0)
        {
            ++source_nonzero;
        }

        output[0][i] = (int16) clipped_l;
        output[1][i] = (int16) clipped_r;
        smsbare_audio_ring_push((int16) mono);
    }

    ++g_smsbare_audio_frame_seq;
    ++g_smsbare_audio_write_calls;
    g_smsbare_audio_samples_written += (unsigned) length;
    g_smsbare_audio_last_render_batch = (unsigned) length;
    if ((unsigned) length > g_smsbare_audio_max_render_batch)
    {
        g_smsbare_audio_max_render_batch = (unsigned) length;
    }
    g_smsbare_audio_source_peak = source_peak;
    g_smsbare_audio_source_nonzero = source_nonzero;
}

void smsplus_bare_sound_configure(void)
{
    memset(&snd, 0, sizeof(snd));
    sms.use_fm = sms_backend_get_fm_music_enabled() ? 1 : 0;
    snd.fps = 60;
    snd.sample_rate = SmsBareAudioSampleRate;
    snd.fm_which = SND_EMU2413;
    snd.fm_clock = SmsBareFmClock;
    snd.psg_clock = SmsBarePsgClock;
    snd.mixer_callback = smsplus_bare_mixer_callback;
    g_smsbare_audio_frame_seq = 0u;
    smsbare_audio_reset_ring();
}

void smsplus_bare_audio_frame_ready(void)
{
    /* The mixer callback marks audio-ready frames when sound_update finishes. */
}

void smsplus_bare_audio_reset_buffer(void)
{
    smsbare_audio_reset_ring();
}

unsigned smsplus_bare_audio_pull(int16_t *dst, unsigned max_samples)
{
    if (dst == 0)
    {
        return max_samples;
    }

    g_smsbare_audio_last_pull_requested = max_samples;
    g_smsbare_audio_last_level_before_pull = g_smsbare_audio_level;
    unsigned pulled = 0u;
    for (unsigned i = 0u; i < max_samples; ++i)
    {
        if (snd.enabled && g_smsbare_audio_level != 0u)
        {
            dst[i] = g_smsbare_audio_ring[g_smsbare_audio_rpos];
            g_smsbare_audio_rpos = (g_smsbare_audio_rpos + 1u) % SmsBareAudioRingSamples;
            --g_smsbare_audio_level;
            ++pulled;
        }
        else
        {
            dst[i] = 0;
            ++g_smsbare_audio_underrun_samples;
        }
    }
    if (pulled < max_samples)
    {
        ++g_smsbare_audio_short_pulls;
    }
    g_smsbare_audio_samples_pulled += pulled;
    g_smsbare_audio_last_pull_returned = pulled;
    g_smsbare_audio_last_level_after_pull = g_smsbare_audio_level;
    return max_samples;
}

void smsplus_bare_fill_audio_stats(SmsBackendAudioStats *stats)
{
    if (stats == 0)
    {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    stats->write_calls = g_smsbare_audio_write_calls;
    stats->samples_written = g_smsbare_audio_samples_written;
    stats->samples_pulled = g_smsbare_audio_samples_pulled;
    stats->ring_level = g_smsbare_audio_level;
    stats->ring_peak = g_smsbare_audio_ring_peak;
    stats->drop_count = g_smsbare_audio_drop_count;
    stats->overwrite_samples = g_smsbare_audio_overwrite_samples;
    stats->short_pulls = g_smsbare_audio_short_pulls;
    stats->underrun_samples = g_smsbare_audio_underrun_samples;
    stats->source_peak = g_smsbare_audio_source_peak;
    stats->source_nonzero = g_smsbare_audio_source_nonzero;
    stats->last_pull_requested = g_smsbare_audio_last_pull_requested;
    stats->last_pull_returned = g_smsbare_audio_last_pull_returned;
    stats->last_level_before_pull = g_smsbare_audio_last_level_before_pull;
    stats->last_level_after_pull = g_smsbare_audio_last_level_after_pull;
    stats->last_render_batch = g_smsbare_audio_last_render_batch;
    stats->max_render_batch = g_smsbare_audio_max_render_batch;
}

void system_manage_sram(uint8 *sram, int slot, int mode)
{
    (void) sram;
    (void) slot;
    (void) mode;
}
