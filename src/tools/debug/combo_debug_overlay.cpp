#include "combo_debug_overlay.h"

#ifndef SMSBARE_BUILD_BID
#define SMSBARE_BUILD_BID ""
#endif

static unsigned BuildHash16FromString(const char *text)
{
    unsigned hash = 2166136261u; /* FNV-1a 32-bit basis */
    if (text == 0)
    {
        return 0u;
    }
    while (*text != '\0')
    {
        hash ^= (unsigned) (u8) *text++;
        hash *= 16777619u; /* FNV prime */
    }
    hash ^= (hash >> 16);
    return hash & 0xFFFFu;
}

static unsigned BuildHash16(void)
{
    static unsigned cached = 0xFFFFFFFFu;
    if (cached == 0xFFFFFFFFu)
    {
        cached = BuildHash16FromString(SMSBARE_BUILD_BID);
    }
    return cached;
}

CComboDebugOverlay::CComboDebugOverlay(void)
: m_WriteFn(0)
, m_Context(0)
{
}

void CComboDebugOverlay::SetWriter(TWriteFn write_fn, void *context)
{
    m_WriteFn = write_fn;
    m_Context = context;
}

void CComboDebugOverlay::Write(const char *text)
{
    if (m_WriteFn != 0 && text != 0)
    {
        m_WriteFn(text, m_Context);
    }
}

void CComboDebugOverlay::Clear(void)
{
    Write("\x1b[1;1H                                                                                                                                ");
    Write("\x1b[2;1H                                                                                                                                ");
    Write("\x1b[3;1H                                                                                                                                ");
    Write("\x1b[4;1H                                                                                                                                ");
    Write("\x1b[5;1H                                                                                                                                ");
    Write("\x1b[6;1H                                                                                                                                ");
    Write("\x1b[7;1H                                                                                                                                ");
    Write("\x1b[8;1H                                                                                                                                ");
    Write("\x1b[9;1H                                                                                                                                ");
    Write("\x1b[10;1H                                                                                                                               ");
    Write("\x1b[11;1H                                                                                                                               ");
    Write("\x1b[12;1H                                                                                                                               ");
    Write("\x1b[13;1H                                                                                                                               ");
    Write("\x1b[14;1H                                                                                                                               ");
    Write("\x1b[15;1H                                                                                                                               ");
    Write("\x1b[16;1H                                                                                                                               ");
    Write("\x1b[0m");
}

void CComboDebugOverlay::Render(const TComboDebugOverlayData &d)
{
    CString line;
    const unsigned os_tick = d.tick % 1000u;
    const unsigned build_hash16 = BuildHash16();

    line.Format("\x1b[97m\x1b[49m\x1b[1;1HTXTDBG BUILD:%04X OS:%03u M:%u C:%u I:%u PAU:%u      ",
        build_hash16 & 0xFFFFu, os_tick, d.mmu_enabled, d.dcache_enabled, d.icache_enabled, d.paused);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[2;1HCI R:%u F:%u K:%u S:%u H:%u/%u P:%05u ",
        d.queue_avail, d.queue_free, d.keyboard_attached, d.sound_active,
        d.sound_start_attempts, d.sound_start_failures, d.perf_tick);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[3;1HEV1 Q:%06u E:%06u A:%04u/%04u S:%04u        ",
        d.audio_write_ops, d.audio_drop_ops, d.samples_written_mod, d.samples_pulled_mod, d.source_peak);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[4;1HEV2 P:%04u C:%05u G:%03u V:%02u.%u KH:%02X KM:%02X ",
        d.audio_peak, d.clip_max, d.audio_gain_ui,
        (d.fps_x10 / 10u) > 99u ? 99u : (d.fps_x10 / 10u), d.fps_x10 % 10u,
        d.last_hid & 0xFFu, d.last_runtime & 0xFFu);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[5;1HJY1 BJ:%02X MJ:%02X R:%u E:%d I:%03u          ",
        d.joy_bridge, d.joy_mapped, d.backend_ready, d.backend_last_error, d.initramfs_files);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[6;1HJY2 F:%06lu PC:%04X R:%04u L:%04u JP:%03u/%03u      ",
        d.backend_frames, d.backend_last_pc & 0xFFFFu, d.backend_pc_repeat_count,
        d.ring_level, d.pacing_err_avg, d.pacing_err_max);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[7;1HSD1 SV:%02u C:%03u A:%04u LD:%04u BT:%04u         ",
        d.save_stage, d.save_code, d.save_aux, d.load_diag, d.boot_to_backend_ms);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[8;1HSD2 MA:%02u MB:%02u CA:%u CB:%u BM:%u MP:%u      ",
        d.mapper_a, d.mapper_b, d.cart_a_loaded, d.cart_b_loaded, d.active_boot_mode, d.active_model_profile);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[9;1HAC1 FM:%u/%u SCC:%u/%u AG:%u AE:%03u M:%u P7:%02X MS:%05X",
        d.fm_music_enabled, d.fm_mix_enabled, d.scc_cart_enabled, d.scc_mix_enabled,
        d.audio_gain_ui, d.audio_gain_effective, d.audio_muted, d.psg_mixer & 0xFFu,
        d.master_switch & 0xFFFFFu);
    Write(line);

    line.Format("\x1b[97m\x1b[49m\x1b[10;1HAC2 L1:%u PSG:%u L2:%u FM:%u L3:%u SCC:%u         ",
        d.layer1_muted, d.psg_mix_enabled, d.layer2_muted, d.fm_mix_enabled, d.layer3_muted, d.scc_mix_enabled);
    Write(line);

    Write("\x1b[0m");
}
