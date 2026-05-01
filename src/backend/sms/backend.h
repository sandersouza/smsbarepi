#ifndef SMSBARE_BACKEND_SMS_BACKEND_H
#define SMSBARE_BACKEND_SMS_BACKEND_H

#include "../core/module.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SMS_CHEAT_CATALOG_PATH "SD:/sms-cheats.cht"
#define SMS_CHEATS_ROOT "SD:/sms/cheats"

#define SMS_CHEAT_MAX_GAMES 4u
#define SMS_CHEAT_MAX_CHEATS 160u
#define SMS_CHEAT_MAX_POKES 256u
#define SMS_CHEAT_TITLE_MAX 96u
#define SMS_CHEAT_ROM_ID_MAX 40u

enum {
    SMS_BACKEND_CAP_STATE = 1u << 0,
    SMS_BACKEND_CAP_MACHINE = 1u << 1,
    SMS_BACKEND_CAP_MEDIA = 1u << 2,
    SMS_BACKEND_CAP_CHEAT = 1u << 3,
    SMS_BACKEND_CAP_DEBUG = 1u << 4
};

enum {
    SMS_BACKEND_INIT_OK = 0,
    SMS_BACKEND_INIT_ERR_CORE = 1
};

typedef BackendRuntimeStatus SmsBackendRuntimeStatus;
typedef BackendVideoPresentation SmsBackendVideoPresentation;
typedef BackendInfo SmsBackendInfo;

typedef enum
{
    SmsCheatLoadOk = 0,
    SmsCheatLoadFileOpenFailed = 1,
    SmsCheatLoadCatalogEmpty = 2,
    SmsCheatLoadLineTooLong = 3,
    SmsCheatLoadSyntaxError = 4,
    SmsCheatLoadUnexpectedKeyword = 5,
    SmsCheatLoadUnexpectedEnd = 6,
    SmsCheatLoadMissingGame = 7,
    SmsCheatLoadMissingCheat = 8,
    SmsCheatLoadDuplicateGame = 9,
    SmsCheatLoadDuplicateCheat = 10,
    SmsCheatLoadDuplicateRomId = 11,
    SmsCheatLoadTooManyGames = 12,
    SmsCheatLoadTooManyCheats = 13,
    SmsCheatLoadTooManyPokes = 14,
    SmsCheatLoadInvalidHex = 15,
    SmsCheatLoadInvalidSize = 16,
    SmsCheatLoadValueOutOfRange = 17,
    SmsCheatLoadTitleTooLong = 18
} TSmsCheatLoadStatus;

typedef struct
{
    uint16_t address;
    uint16_t value;
    uint8_t size_bits;
} TSmsCheatPoke;

typedef struct
{
    char title[SMS_CHEAT_TITLE_MAX + 1u];
    uint16_t poke_first;
    uint16_t poke_count;
} TSmsCheat;

typedef struct
{
    char title[SMS_CHEAT_TITLE_MAX + 1u];
    char rom_id[SMS_CHEAT_ROM_ID_MAX + 1u];
    uint16_t cheat_first;
    uint16_t cheat_count;
} TSmsCheatGame;

typedef struct
{
    TSmsCheatGame games[SMS_CHEAT_MAX_GAMES];
    TSmsCheat cheats[SMS_CHEAT_MAX_CHEATS];
    TSmsCheatPoke pokes[SMS_CHEAT_MAX_POKES];
    uint16_t game_count;
    uint16_t cheat_count;
    uint16_t poke_count;
    uint16_t error_line;
    uint8_t last_status;
} TSmsCheatCatalog;

typedef struct
{
    unsigned id;
    const char *key;
    const char *label;
    const char *vdp_chip;
    const char *default_video_timing;
    const char *default_audio;
    const char *default_roms;
    unsigned default_boot_mode;
    unsigned default_disk_rom_enabled;
    unsigned default_refresh_hz;
    unsigned default_rammapper_kb;
    unsigned default_megaram_kb;
    unsigned default_z80_turbo_mode;
    unsigned default_fm_music_enabled;
    unsigned default_scc_cart_enabled;
    unsigned is_sms_family;
} TSmsMachineProfileSpec;

typedef struct { unsigned unused; } SmsBackendVideoHostDebug;

typedef struct {
    unsigned write_calls;
    unsigned samples_written;
    unsigned samples_pulled;
    unsigned ring_level;
    unsigned ring_peak;
    unsigned drop_count;
    unsigned overwrite_samples;
    unsigned trim_events;
    unsigned trim_samples;
    unsigned short_pulls;
    unsigned underrun_samples;
    unsigned source_peak;
    unsigned source_nonzero;
    unsigned last_pull_requested;
    unsigned last_pull_returned;
    unsigned last_level_before_pull;
    unsigned last_level_after_pull;
    unsigned last_trim_samples;
    unsigned last_render_batch;
    unsigned max_render_batch;
} SmsBackendAudioStats;

typedef struct {
    uint32_t frame_number;
    unsigned sample_count;
    unsigned master_switch;
    unsigned bus_active_mask;
    unsigned fm_backend_nukeykt;
    unsigned psg_peak;
    unsigned fm_peak;
    unsigned scc_primary_peak;
    unsigned scc_secondary_peak;
    unsigned main_peak;
} SmsBackendAudioMixDebug;

typedef struct {
    uint32_t frame_number;
    unsigned psg_mask;
    unsigned fm_mask;
    unsigned scc_primary_mask;
    unsigned scc_secondary_mask;
} SmsBackendAudioChannelDebug;

typedef struct {
    unsigned last_hid;
    unsigned last_sms;
    unsigned runtime_queued;
    unsigned runtime_high_water;
    unsigned runtime_dropped;
    unsigned candidate_created;
    unsigned candidate_promoted;
    unsigned candidate_discarded;
    unsigned immediate_releases;
    unsigned immediate_gameplay_presses;
} SmsBackendKeyboardDebug;

enum {
    SMS_VDP_TRACE_NONE = 0,
    SMS_VDP_TRACE_TITLE = 1,
    SMS_VDP_TRACE_SAVE_SELECT = 2,
    SMS_VDP_TRACE_STARDATE = 3,
    SMS_VDP_TRACE_KMG = 4
};

enum {
    SMS_VDP_EVENT_NONE = 0,
    SMS_VDP_EVENT_IE0_FIRE = 1,
    SMS_VDP_EVENT_IE1_ARM = 2,
    SMS_VDP_EVENT_IE1_FIRE = 3,
    SMS_VDP_EVENT_R19_WRITE = 4,
    SMS_VDP_EVENT_R23_WRITE = 5,
    SMS_VDP_EVENT_S1_READ = 6,
    SMS_VDP_EVENT_S2_READ = 7,
    SMS_VDP_EVENT_CMD_START = 8,
    SMS_VDP_EVENT_CMD_END = 9,
    SMS_VDP_EVENT_PORT98_WRITE = 10,
    SMS_VDP_EVENT_PORT99_ADDR = 11,
    SMS_VDP_EVENT_PORT99_REG = 12
};

enum {
    SMS_AUX_TRACE_NONE = 0,
    SMS_AUX_TRACE_WAVE = 1,
    SMS_AUX_TRACE_FREQ = 2,
    SMS_AUX_TRACE_VOLUME = 3,
    SMS_AUX_TRACE_ENABLE = 4,
    SMS_AUX_TRACE_DEFORM = 5
};

typedef struct {
    uint32_t seq;
    uint32_t frame;
    uint16_t scanline;
    uint16_t pc;
    uint8_t line_phase;
    uint8_t event_type;
    uint8_t capture_window;
    uint8_t scr_mode;
    uint8_t reg1;
    uint8_t reg2;
    uint8_t reg8;
    uint8_t reg9;
    uint8_t reg15;
    uint8_t reg19;
    uint8_t reg23;
    uint8_t reg25;
    uint8_t reg26;
    uint8_t reg27;
    uint8_t status0;
    uint8_t status1;
    uint8_t status2;
    uint8_t irq_pending;
    uint8_t screen_on;
    uint8_t ie0_enabled;
    uint8_t ie1_enabled;
    uint8_t hr;
    uint8_t vr;
    uint8_t ie1_target_line;
    int8_t ie1_target_delta;
    uint8_t effective_odd_page;
    uint8_t render_page;
    uint8_t cart_slot_p1;
    uint8_t cart_slot_p2;
    uint8_t cart_bank_p1_lo;
    uint8_t cart_bank_p1_hi;
    uint8_t cart_bank_p2_lo;
    uint8_t cart_bank_p2_hi;
    uint8_t vdp_cmd;
    uint8_t vdp_ce;
    uint8_t aux_reg;
    uint8_t aux_value;
    uint8_t aux_ctrl;
    uint8_t reserved0;
    uint32_t aux_addr;
} SmsBackendVdpTraceEvent;

typedef struct {
    const SmsBackendVdpTraceEvent *events;
    unsigned capacity;
    unsigned count;
    unsigned head;
    unsigned dropped;
} SmsBackendVdpTraceView;

typedef struct {
    uint32_t seq;
    uint32_t frame;
    uint16_t pc;
    uint16_t addr_first;
    uint16_t addr_last;
    uint16_t count;
    uint16_t rendered_samples_before;
    uint8_t value_first;
    uint8_t value_last;
    uint8_t slot;
    uint8_t chip;
    uint8_t ps;
    uint8_t ss;
    uint8_t bank16;
    uint8_t kind;
    uint8_t mode;
    uint8_t mapper;
    uint8_t capture_window;
    uint8_t session;
} SmsBackendAuxTraceEvent;

typedef struct {
    const SmsBackendAuxTraceEvent *events;
    unsigned capacity;
    unsigned count;
    unsigned head;
    unsigned dropped;
} SmsBackendAuxTraceView;

typedef struct {
    uint32_t seq;
    uint32_t frame;
    uint16_t pc;
    uint16_t count;
    uint8_t reg;
    uint8_t value_first;
    uint8_t value_last;
    uint8_t chip;
    uint8_t op;
    uint8_t capture_window;
    uint8_t session;
} SmsBackendAudioPortTraceEvent;

typedef struct {
    const SmsBackendAudioPortTraceEvent *events;
    unsigned capacity;
    unsigned count;
    unsigned head;
    unsigned dropped;
} SmsBackendAudioPortTraceView;

typedef struct {
    uint32_t seq;
    uint32_t frame;
    uint16_t pc;
    uint16_t addr;
    uint8_t value;
    uint8_t l220e;
    uint8_t l220f;
    uint8_t l2212;
    uint8_t l2219;
} SmsBackendKmgTraceEvent;

typedef struct {
    const SmsBackendKmgTraceEvent *events;
    unsigned capacity;
    unsigned count;
    unsigned head;
    unsigned dropped;
} SmsBackendKmgTraceView;

typedef struct {
    unsigned h_period;
    unsigned v_period;
    unsigned i_period;
    unsigned scanline;
    unsigned pal_video;
    unsigned lines_212;
    unsigned turbo_percent;
    unsigned scr_mode;
    unsigned reg2;
    unsigned reg9;
    unsigned reg25;
    unsigned reg26;
    unsigned reg27;
    unsigned reg17;
    unsigned reg19;
    unsigned reg1;
    unsigned reg7;
    unsigned status8;
    unsigned status9;
    unsigned last_status_read_reg;
    unsigned last_status_read_value;
    unsigned last_port99_op;
    unsigned last_port99_value;
    unsigned last_port99_alatch;
    unsigned last_port99_vkey;
    unsigned last_port99_r15;
    unsigned last_port99_pc;
    unsigned screen_on;
    unsigned ie0_enabled;
    unsigned vblank_flag;
    unsigned ie1_enabled;
    unsigned ie1_flag;
    unsigned status2_hr;
    unsigned status2_vr;
    unsigned line_phase;
    unsigned ie1_target_line;
    int ie1_target_delta;
    unsigned effective_odd_page;
    unsigned render_page;
    unsigned cart_slot_p1;
    unsigned cart_slot_p2;
    unsigned cart_bank_p1_lo;
    unsigned cart_bank_p1_hi;
    unsigned cart_bank_p2_lo;
    unsigned cart_bank_p2_hi;
    unsigned slot_reg;
    unsigned p1_write_enabled;
    unsigned cart_type_p1;
    unsigned cart_type_source_p1;
    unsigned cart_last_type_slot;
    unsigned cart_last_type_value;
    unsigned cart_last_type_source;
    unsigned kanji_basic;
    unsigned kanji_basic_map_ok;
    unsigned kanji_basic_selected_p1;
    unsigned kanji_basic_seen_p1;
    unsigned p1_ps;
    unsigned p1_ss;
    unsigned disk_rom_enabled;
    unsigned disk_rom_loaded;
    unsigned kanji_ime_loaded;
    unsigned kanji_ime_selected_p1;
    unsigned kanji_ime_seen_p1;
    unsigned rtc_reads;
    unsigned rtc_writes;
    unsigned rtc_last_reg;
    unsigned rtc_mode;
    unsigned hscroll;
    unsigned hscroll512;
    unsigned multipage_enabled;
    unsigned display_page;
    unsigned vdp_cmd;
    unsigned vdp_ce;
    unsigned vdp_tr;
    unsigned vdp_cmd_starts;
    unsigned vdp_copy_starts;
    unsigned mode_yjk;
    unsigned mode_yae;
    unsigned ie1_events;
    unsigned ie1_acks;
    unsigned ie1_armed;
    unsigned ie1_fired_hblank;
    unsigned vdp_r9_writes;
    unsigned vdp_r15_writes;
    unsigned vdp_r18_writes;
    unsigned vdp_r19_writes;
    unsigned vdp_r25_writes;
    unsigned vdp_r26_writes;
    unsigned vdp_r27_writes;
    unsigned vdp_s0_reads;
    unsigned vdp_s1_reads;
    unsigned vdp_s2_reads;
    unsigned vdp_port99_reads;
    unsigned vdp_port99_writes;
    unsigned vdp_port9b_writes;
    unsigned vdp_port9b_last_reg17_before;
    unsigned vdp_port9b_last_target;
    unsigned vdp_port9b_last_reg17_after;
    unsigned vdp_port9b_last_value;
    unsigned trace_last_event_type;
    unsigned trace_last_window;
    unsigned trace_last_pc;
    unsigned trace_last_scanline;
    unsigned trace_last_line_phase;
    unsigned trace_last_reg1;
    unsigned trace_last_reg23;
    unsigned trace_last_status2;
    unsigned trace_last_bank_p1_lo;
    unsigned trace_last_bank_p2_lo;
    unsigned flash_last_op;
    unsigned flash_last_slot;
    unsigned flash_last_addr;
    unsigned flash_last_offset;
    unsigned flash_last_value;
    unsigned flash_last_read_value;
    unsigned flash_last_state;
    unsigned flash_last_mode;
    unsigned flash_program_count;
    unsigned flash_erase_count;
    unsigned flash_read_count;
    unsigned kanji_available;
    unsigned kanji_reads;
    unsigned kanji_addr_writes;
    unsigned kanji_null_reads;
    unsigned fdc_status;
    unsigned fdc_track_reg;
    unsigned fdc_sector_reg;
    unsigned fdc_drive;
    unsigned fdc_side;
    unsigned fdc_irq;
    unsigned fdc_wait;
    unsigned kbd_runtime_queued;
    unsigned kbd_runtime_high_water;
    unsigned kbd_runtime_dropped;
    unsigned kbd_candidate_created;
    unsigned kbd_candidate_promoted;
    unsigned kbd_candidate_discarded;
    unsigned kbd_immediate_releases;
    unsigned kbd_immediate_gameplay_presses;
    unsigned cpu_frame_cycles;
    unsigned cpu_total_cycles;
    unsigned cpu_instructions;
    unsigned cpu_clock_multiplier;
    unsigned cpu_sync_waiting;
    unsigned cpu_irq_pending;
    unsigned cpu_nmi_sources;
    unsigned cpu_firq_sources;
    unsigned cpu_irq_sources;
    unsigned gime_irq_latch;
    unsigned gime_irq_enable;
    unsigned gime_firq_latch;
    unsigned gime_firq_enable;
    unsigned gime_enhanced_flags;
    unsigned gime_timer_rate;
    unsigned gime_timer_armed;
    unsigned gime_timer_counter;
    unsigned gime_timer_reload;
    unsigned pia0_cra;
    unsigned pia0_crb;
    unsigned pia1_crb;
    unsigned disk_nmi_pending;
    unsigned disk_interrupt_enable;
    unsigned cpu_last_opcode;
    unsigned cpu_last_prefix;
    unsigned cpu_last_postbyte;
    unsigned cpu_last_ea_cycles;
    unsigned cpu_write_cycle_offset;
    unsigned cpu_trace_last_event;
    unsigned cpu_trace_count;
    unsigned audio_ring_level;
    unsigned audio_drop_count;
    unsigned audio_sample_accum;
} SmsBackendTimingDebug;

const char *sms_backend_family_id(void);
const char *sms_backend_default_id(void);
void sms_backend_set_requested_backend_id(const char *backend_id);
const char *sms_backend_requested_backend_id(void);
const char *sms_backend_selected_backend_id(void);
boolean sms_backend_is_capability_available(unsigned capability_bit);

boolean sms_backend_init(void);
boolean sms_backend_reset(void);
boolean sms_backend_step_frame(void);
void sms_backend_shutdown(void);
unsigned sms_backend_video_frame(const u16 **pixels, unsigned *width, unsigned *height);
void sms_backend_get_video_presentation(SmsBackendVideoPresentation *presentation);
unsigned sms_backend_audio_pull(int16_t *dst, unsigned max_samples);
void sms_backend_input_push_joystick(u8 bridge_bits);
void sms_backend_input_push_keyboard(u8 modifiers, const unsigned char raw_keys[6]);
boolean sms_backend_media_load_cartridge(unsigned slot_index, const char *path);
boolean sms_backend_media_load_disk(unsigned drive_index, const char *path);
boolean sms_backend_media_load_cassette(const char *path);
SmsBackendRuntimeStatus sms_backend_get_status(void);
SmsBackendInfo sms_backend_get_info(void);

boolean sms_backend_reboot_basic(void);
boolean sms_backend_menu_reset(void);
boolean sms_backend_save_state(void *dst, unsigned capacity, unsigned *saved_size);
boolean sms_backend_load_state(const void *src, unsigned size);

void sms_backend_set_overscan(boolean enabled);
void sms_backend_set_refresh_hz(unsigned hz);
unsigned sms_backend_get_refresh_hz(void);
uint64_t sms_backend_get_frame_interval_us_exact(void);
void sms_backend_set_disk_rom_enabled(boolean enabled);
boolean sms_backend_get_disk_rom_enabled(void);
void sms_backend_set_color_artifacts_enabled(boolean enabled);
boolean sms_backend_get_color_artifacts_enabled(void);
void sms_backend_set_gfx9000_enabled(boolean enabled);
boolean sms_backend_get_gfx9000_enabled(void);
void sms_backend_set_boot_mode(unsigned mode);
unsigned sms_backend_get_boot_mode(void);
unsigned sms_backend_get_active_boot_mode(void);
void sms_backend_set_model_profile(unsigned profile);
unsigned sms_backend_get_model_profile(void);
unsigned sms_backend_get_active_model_profile(void);
void sms_backend_set_processor_mode(unsigned mode);
unsigned sms_backend_get_processor_mode(void);
unsigned sms_backend_get_active_processor_mode(void);
void sms_backend_set_rammapper_kb(unsigned kb);
unsigned sms_backend_get_rammapper_kb(void);
void sms_backend_set_megaram_kb(unsigned kb);
unsigned sms_backend_get_megaram_kb(void);
void sms_backend_set_z80_turbo_mode(unsigned mode);
unsigned sms_backend_get_z80_turbo_mode(void);
void sms_backend_set_fm_music_enabled(boolean enabled);
boolean sms_backend_get_fm_music_enabled(void);
void sms_backend_set_scc_plus_enabled(boolean enabled);
boolean sms_backend_get_scc_plus_enabled(void);
boolean sms_backend_set_scc_dual_cart_enabled(boolean enabled);
boolean sms_backend_get_scc_dual_cart_enabled(void);
boolean sms_backend_get_scc_dual_cart_available(void);
void sms_backend_set_fm_layer_gain_pct(unsigned pct);
unsigned sms_backend_get_fm_layer_gain_pct(void);
void sms_backend_set_audio_layer_muted(unsigned layer_index, boolean muted);
boolean sms_backend_get_audio_layer_muted(unsigned layer_index);
unsigned sms_backend_get_psg_mixer_reg7(void);
unsigned sms_backend_get_master_switch(void);
void sms_backend_set_frame_perf_telemetry_enabled(boolean enabled);
void sms_backend_reset_audio_mix_debug_window(void);

const char *sms_backend_get_cartridge_name(unsigned slot_index);
const char *sms_backend_get_disk_name(unsigned drive_index);
const char *sms_backend_get_cassette_name(void);
boolean sms_backend_unload_cartridge(unsigned slot_index);
boolean sms_backend_unload_disk(unsigned drive_index);
boolean sms_backend_unload_cassette(void);

void sms_backend_reset_cheat_session(void);
boolean sms_backend_set_cheat_enabled(const TSmsCheatPoke *pokes, unsigned poke_count, boolean enabled);
boolean sms_backend_set_cheat_value(const TSmsCheatPoke *pokes, unsigned poke_count, uint16_t value);
boolean sms_backend_is_cheat_active(const TSmsCheatPoke *pokes, unsigned poke_count);
boolean sms_backend_peek8(uint16_t address, uint8_t *value);
boolean sms_backend_peek16(uint16_t address, uint16_t *value);

void sms_backend_set_load_diag(unsigned stage);
unsigned sms_backend_get_load_diag(void);
void sms_backend_get_video_host_debug(SmsBackendVideoHostDebug *debug);
void sms_backend_get_audio_stats(SmsBackendAudioStats *stats);
void sms_backend_get_audio_mix_debug(SmsBackendAudioMixDebug *stats);
void sms_backend_get_audio_channel_debug(SmsBackendAudioChannelDebug *stats);
void sms_backend_get_keyboard_debug(SmsBackendKeyboardDebug *stats);
void sms_backend_get_timing_debug(SmsBackendTimingDebug *stats);
void sms_backend_get_vdp_trace_view(SmsBackendVdpTraceView *view);
void sms_backend_get_audio_port_trace_view(SmsBackendAudioPortTraceView *view);
void sms_backend_get_kmg_trace_view(SmsBackendKmgTraceView *view);

unsigned sms_initramfs_mount(void);
unsigned sms_initramfs_mount_boot_minimal(void);
unsigned sms_initramfs_mount_remaining(void);
unsigned sms_initramfs_files_mounted(void);
const char *sms_initramfs_source(void);
unsigned sms_initramfs_bytes(void);

void sms_cheat_catalog_reset(TSmsCheatCatalog *catalog);
TSmsCheatLoadStatus sms_cheat_catalog_load(TSmsCheatCatalog *catalog, const char *path);
TSmsCheatLoadStatus sms_cheat_catalog_load_default(TSmsCheatCatalog *catalog);
TSmsCheatLoadStatus sms_cheat_catalog_load_mcf(TSmsCheatCatalog *catalog, const char *path);
const char *sms_cheat_catalog_status_name(TSmsCheatLoadStatus status);

unsigned sms_machine_profile_count(void);
unsigned sms_machine_profile_clamp(unsigned profile);
const TSmsMachineProfileSpec *sms_machine_profile_get(unsigned profile);
const TSmsMachineProfileSpec *sms_machine_profile_default(void);
const char *sms_machine_profile_label(unsigned profile);
boolean sms_machine_profile_is_sms_family(unsigned profile);

extern const BackendModuleV1 g_sms_backend_module_smsplus;

#ifdef __cplusplus
}
#endif

#endif
