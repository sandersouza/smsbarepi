#include "backend.h"

#include "../core/registry.h"

#include <circle/startup.h>
#include <fatfs/ff.h>
#include <string.h>

#ifndef SMSBARE_BACKEND_DEFAULT
#define SMSBARE_BACKEND_DEFAULT "sms.smsplus"
#endif

extern void smsbare_uart_write(const char *text);
extern unsigned long long smsbare_monotonic_time_us(void);
extern boolean smsplus_media_load_cartridge_bytes(const char *label, const uint8_t *data, unsigned size);
extern void smsplus_bare_set_fm_enabled(boolean enabled);
extern void smsplus_bare_fill_audio_stats(SmsBackendAudioStats *stats);

static const char *sms_backend_basename(const char *path);

static const char kSmsBackendFamilyId[] = "sms";
static char g_sms_requested_backend_id[32];
static char g_sms_cart_name[32];
static unsigned g_sms_refresh_hz = 60u;
static unsigned g_sms_load_diag = 0u;
static boolean g_sms_audio_layer_muted[4] = { FALSE, TRUE, TRUE, TRUE };
static const unsigned char *g_sms_initramfs_default_rom = 0;
static unsigned g_sms_initramfs_default_rom_size = 0u;
static unsigned g_sms_initramfs_files = 0u;
static const char *g_sms_initramfs_source = "none";
static unsigned g_sms_initramfs_bytes = 0u;
static boolean g_sms_user_cartridge_loaded = FALSE;

enum {
    kSmsMaxCpioEntries = 128u,
    kSmsMaxCpioNameSize = 4096u,
    kSmsMaxInitrdSize = 64u * 1024u * 1024u
};

static const BackendModuleV1 *kSmsBuiltinModules[] = {
    &g_sms_backend_module_smsplus
};

static BackendRegistry g_sms_registry;
static BackendInfo g_sms_backend_info = {
    "sms",
    0,
    0,
    "smsplus",
    FALSE,
    FALSE,
    FALSE,
    FALSE,
    SMS_BACKEND_INIT_OK
};

static const BackendHostExportsV1 kSmsHostExports = {
    BACKEND_MODULE_ABI_V1,
    smsbare_uart_write,
    smsbare_monotonic_time_us
};

static unsigned sms_backend_align4(unsigned value)
{
    return (value + 3u) & ~3u;
}

static unsigned long long sms_backend_align4_u64(unsigned long long value)
{
    return (value + 3ull) & ~3ull;
}

static unsigned sms_backend_be32(const u8 *p)
{
    return ((unsigned) p[0] << 24)
         | ((unsigned) p[1] << 16)
         | ((unsigned) p[2] << 8)
         | (unsigned) p[3];
}

static unsigned sms_backend_strlen(const char *text)
{
    unsigned len = 0u;
    if (text == 0)
    {
        return 0u;
    }
    while (text[len] != '\0')
    {
        ++len;
    }
    return len;
}

static int sms_backend_streq(const char *a, const char *b)
{
    unsigned i = 0u;
    if (a == 0 || b == 0)
    {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
        {
            return 0;
        }
        ++i;
    }
    return (a[i] == '\0' && b[i] == '\0') ? 1 : 0;
}

static char sms_backend_to_upper_ascii(char ch)
{
    return (ch >= 'a' && ch <= 'z') ? (char) (ch - 'a' + 'A') : ch;
}

static int sms_backend_str_ieq(const char *a, const char *b)
{
    unsigned i = 0u;
    if (a == 0 || b == 0)
    {
        return 0;
    }
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (sms_backend_to_upper_ascii(a[i]) != sms_backend_to_upper_ascii(b[i]))
        {
            return 0;
        }
        ++i;
    }
    return (a[i] == '\0' && b[i] == '\0') ? 1 : 0;
}

static unsigned sms_backend_hex_to_u32(const char *text, unsigned len)
{
    unsigned value = 0u;
    for (unsigned i = 0u; i < len; ++i)
    {
        const char c = text[i];
        unsigned digit = 0u;
        if (c >= '0' && c <= '9') digit = (unsigned) (c - '0');
        else if (c >= 'a' && c <= 'f') digit = (unsigned) (10 + c - 'a');
        else if (c >= 'A' && c <= 'F') digit = (unsigned) (10 + c - 'A');
        else return 0u;
        value = (value << 4) | digit;
    }
    return value;
}

static int sms_backend_address_range_is_sane(unsigned long long start, unsigned long long end)
{
    if (start < 0x100ull || end <= start || end >= 0x40000000ull)
    {
        return 0;
    }
    if ((end - start) > (unsigned long long) kSmsMaxInitrdSize)
    {
        return 0;
    }
    return 1;
}

static int sms_backend_fdt_find_initrd(const void *dtb_addr,
                                       unsigned long long *out_start,
                                       unsigned long long *out_end)
{
    if (dtb_addr == 0 || out_start == 0 || out_end == 0)
    {
        return 0;
    }

    const u8 *base = (const u8 *) dtb_addr;
    const unsigned magic = sms_backend_be32(base + 0x00);
    const unsigned total_size = sms_backend_be32(base + 0x04);
    const unsigned off_dt_struct = sms_backend_be32(base + 0x08);
    const unsigned off_dt_strings = sms_backend_be32(base + 0x0C);
    const unsigned size_dt_strings = sms_backend_be32(base + 0x20);
    const unsigned size_dt_struct = sms_backend_be32(base + 0x24);
    if (magic != 0xD00DFEEDu || total_size < 0x28u)
    {
        return 0;
    }
    if (off_dt_struct >= total_size || size_dt_struct == 0u || off_dt_struct + size_dt_struct > total_size)
    {
        return 0;
    }
    if (off_dt_strings >= total_size || size_dt_strings == 0u || off_dt_strings + size_dt_strings > total_size)
    {
        return 0;
    }

    const u8 *struct_ptr = base + off_dt_struct;
    const u8 *struct_end = struct_ptr + size_dt_struct;
    const char *strings = (const char *) (base + off_dt_strings);
    unsigned depth = 0u;
    unsigned chosen_depth = 0u;
    int in_chosen = 0;
    int got_start = 0;
    int got_end = 0;
    unsigned long long initrd_start = 0ull;
    unsigned long long initrd_end = 0ull;

    while ((struct_ptr + 4) <= struct_end)
    {
        const unsigned token = sms_backend_be32(struct_ptr);
        struct_ptr += 4;
        if (token == 1u)
        {
            const char *node_name = (const char *) struct_ptr;
            const unsigned name_len = sms_backend_strlen(node_name) + 1u;
            struct_ptr += sms_backend_align4(name_len);
            if (depth == 1u && sms_backend_streq(node_name, "chosen"))
            {
                in_chosen = 1;
                chosen_depth = depth + 1u;
            }
            depth++;
        }
        else if (token == 2u)
        {
            if (depth > 0u)
            {
                if (in_chosen && depth == chosen_depth)
                {
                    in_chosen = 0;
                    chosen_depth = 0u;
                }
                depth--;
            }
        }
        else if (token == 3u)
        {
            if ((struct_ptr + 8) > struct_end)
            {
                break;
            }
            const unsigned prop_len = sms_backend_be32(struct_ptr);
            const unsigned name_off = sms_backend_be32(struct_ptr + 4);
            struct_ptr += 8;
            const u8 *prop_value = struct_ptr;
            struct_ptr += sms_backend_align4(prop_len);
            if (!in_chosen || name_off >= size_dt_strings)
            {
                continue;
            }
            const char *prop_name = strings + name_off;
            if (sms_backend_streq(prop_name, "linux,initrd-start"))
            {
                if (prop_len >= 8u)
                {
                    initrd_start = ((unsigned long long) sms_backend_be32(prop_value) << 32)
                                 | (unsigned long long) sms_backend_be32(prop_value + 4);
                    got_start = 1;
                }
                else if (prop_len >= 4u)
                {
                    initrd_start = (unsigned long long) sms_backend_be32(prop_value);
                    got_start = 1;
                }
            }
            else if (sms_backend_streq(prop_name, "linux,initrd-end"))
            {
                if (prop_len >= 8u)
                {
                    initrd_end = ((unsigned long long) sms_backend_be32(prop_value) << 32)
                               | (unsigned long long) sms_backend_be32(prop_value + 4);
                    got_end = 1;
                }
                else if (prop_len >= 4u)
                {
                    initrd_end = (unsigned long long) sms_backend_be32(prop_value);
                    got_end = 1;
                }
            }
        }
        else if (token == 9u)
        {
            break;
        }
        else if (token != 4u)
        {
            break;
        }
    }

    if (got_start && got_end && sms_backend_address_range_is_sane(initrd_start, initrd_end))
    {
        *out_start = initrd_start;
        *out_end = initrd_end;
        return 1;
    }
    return 0;
}

static unsigned sms_backend_mount_cpio_from_range(const unsigned char *cursor,
                                                  const unsigned char *limit)
{
    unsigned entries = 0u;
    unsigned mounted = 0u;
    while ((cursor + 110) <= limit)
    {
        const char *header = (const char *) cursor;
        if (!((header[0] == '0' && header[1] == '7' && header[2] == '0'
            && header[3] == '7' && header[4] == '0'
            && (header[5] == '1' || header[5] == '2'))))
        {
            break;
        }
        if (entries++ >= kSmsMaxCpioEntries)
        {
            break;
        }
        const unsigned file_size = sms_backend_hex_to_u32(header + 54, 8u);
        const unsigned name_size = sms_backend_hex_to_u32(header + 94, 8u);
        if (name_size < 2u || name_size > kSmsMaxCpioNameSize)
        {
            break;
        }
        const unsigned long long header_and_name = sms_backend_align4_u64(110ull + (unsigned long long) name_size);
        const unsigned aligned_file_size = sms_backend_align4(file_size);
        if (header_and_name > (unsigned long long) (limit - cursor))
        {
            break;
        }
        const char *name = (const char *) (cursor + 110);
        if (name[name_size - 1u] != '\0')
        {
            break;
        }
        const unsigned char *data = cursor + header_and_name;
        if ((data + file_size) > limit)
        {
            break;
        }
        if (name_size >= 11u && name[0] == 'T' && name[1] == 'R' && name[2] == 'A'
            && name[3] == 'I' && name[4] == 'L' && name[5] == 'E' && name[6] == 'R'
            && name[7] == '!' && name[8] == '!' && name[9] == '!')
        {
            break;
        }
        if (file_size > 0u && sms_backend_str_ieq(sms_backend_basename(name), "bios.sms"))
        {
            g_sms_initramfs_default_rom = data;
            g_sms_initramfs_default_rom_size = file_size;
            g_sms_initramfs_files = 1u;
            mounted = 1u;
        }
        const unsigned long long total_size = header_and_name + (unsigned long long) aligned_file_size;
        if (total_size == 0ull || total_size > (unsigned long long) (limit - cursor))
        {
            break;
        }
        cursor += total_size;
    }
    return mounted;
}

static unsigned sms_backend_mount_external_initramfs(void)
{
    u32 * volatile dtb_ptr32 = (u32 * volatile) ARM_DTB_PTR32;
    const unsigned long long dtb_raw = (unsigned long long) *dtb_ptr32;
    unsigned long long initrd_start = 0ull;
    unsigned long long initrd_end = 0ull;
    if (dtb_raw < 0x100ull || dtb_raw >= 0x40000000ull || (dtb_raw & 0x3ull) != 0ull)
    {
        return 0u;
    }
    if (!sms_backend_fdt_find_initrd((const void *) (uintptr_t) dtb_raw, &initrd_start, &initrd_end))
    {
        return 0u;
    }
    g_sms_initramfs_source = "external";
    g_sms_initramfs_bytes = (unsigned) (initrd_end - initrd_start);
    return sms_backend_mount_cpio_from_range((const unsigned char *) (uintptr_t) initrd_start,
                                             (const unsigned char *) (uintptr_t) initrd_end);
}

static void sms_backend_copy_text(char *dst, unsigned dst_size, const char *src)
{
    unsigned i = 0u;
    if (dst == 0 || dst_size == 0u)
    {
        return;
    }
    if (src == 0)
    {
        src = "";
    }
    while (i + 1u < dst_size && src[i] != '\0')
    {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static boolean sms_backend_load_default_bios(void)
{
    if (g_sms_initramfs_default_rom == 0 || g_sms_initramfs_default_rom_size == 0u)
    {
        return FALSE;
    }
    if (!smsplus_media_load_cartridge_bytes("bios.sms",
                                            g_sms_initramfs_default_rom,
                                            g_sms_initramfs_default_rom_size))
    {
        return FALSE;
    }
    g_sms_cart_name[0] = '\0';
    g_sms_user_cartridge_loaded = FALSE;
    return TRUE;
}

static const char *sms_backend_basename(const char *path)
{
    const char *base = path;
    if (path == 0)
    {
        return "";
    }
    for (const char *p = path; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
        {
            base = p + 1;
        }
    }
    return base;
}

static void sms_backend_registry_bootstrap(void)
{
    static boolean initialized = FALSE;
    if (initialized)
    {
        return;
    }
    backend_registry_init(&g_sms_registry,
                          kSmsBuiltinModules,
                          sizeof(kSmsBuiltinModules) / sizeof(kSmsBuiltinModules[0]),
                          SMSBARE_BACKEND_DEFAULT);
    initialized = TRUE;
}

const char *sms_backend_family_id(void)
{
    return kSmsBackendFamilyId;
}

const char *sms_backend_default_id(void)
{
    sms_backend_registry_bootstrap();
    return g_sms_registry.default_module != 0 ? g_sms_registry.default_module->backend_id : SMSBARE_BACKEND_DEFAULT;
}

void sms_backend_set_requested_backend_id(const char *backend_id)
{
    sms_backend_copy_text(g_sms_requested_backend_id, sizeof(g_sms_requested_backend_id), backend_id);
}

const char *sms_backend_requested_backend_id(void)
{
    return g_sms_requested_backend_id;
}

const char *sms_backend_selected_backend_id(void)
{
    return g_sms_backend_info.selected_backend_id != 0 ? g_sms_backend_info.selected_backend_id : sms_backend_default_id();
}

boolean sms_backend_is_capability_available(unsigned capability_bit)
{
    const BackendModuleV1 *module;
    sms_backend_registry_bootstrap();
    module = g_sms_registry.selected_module != 0 ? g_sms_registry.selected_module : g_sms_registry.default_module;
    return (module != 0 && (module->capability_bits & capability_bit) != 0u) ? TRUE : FALSE;
}

boolean sms_backend_init(void)
{
    boolean requested_found = FALSE;
    boolean used_default = FALSE;
    int init_error = SMS_BACKEND_INIT_OK;
    const BackendModuleV1 *module;

    sms_backend_registry_bootstrap();
    module = backend_registry_select(&g_sms_registry,
                                     g_sms_requested_backend_id[0] != '\0' ? g_sms_requested_backend_id : 0,
                                     &requested_found,
                                     &used_default);

    g_sms_backend_info.requested_backend_id = g_sms_requested_backend_id[0] != '\0' ? g_sms_requested_backend_id : 0;
    g_sms_backend_info.selected_backend_id = module != 0 ? module->backend_id : 0;
    g_sms_backend_info.active_backend_name = module != 0 ? module->display_name : "smsplus";
    g_sms_backend_info.requested_backend_found = requested_found;
    g_sms_backend_info.used_default_backend = used_default;
    g_sms_backend_info.init_attempted = TRUE;
    g_sms_backend_info.available = FALSE;
    g_sms_backend_info.init_error_code = SMS_BACKEND_INIT_ERR_CORE;

    if (module == 0 || module->ops == 0 || module->ops->init == 0)
    {
        return FALSE;
    }

    if (!module->ops->init(&kSmsHostExports, &init_error))
    {
        g_sms_backend_info.init_error_code = init_error;
        return FALSE;
    }

    g_sms_backend_info.available = TRUE;
    g_sms_backend_info.init_error_code = SMS_BACKEND_INIT_OK;
    (void) sms_backend_load_default_bios();
    return TRUE;
}

boolean sms_backend_reset(void)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    return (module != 0 && module->ops != 0 && module->ops->reset != 0) ? module->ops->reset() : FALSE;
}

boolean sms_backend_step_frame(void)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    return (module != 0 && module->ops != 0 && module->ops->step_frame != 0) ? module->ops->step_frame() : FALSE;
}

void sms_backend_shutdown(void)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->shutdown != 0)
    {
        module->ops->shutdown();
    }
}

unsigned sms_backend_video_frame(const u16 **pixels, unsigned *width, unsigned *height)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->video_frame != 0)
    {
        return module->ops->video_frame(pixels, width, height);
    }
    if (pixels != 0) *pixels = 0;
    if (width != 0) *width = 0u;
    if (height != 0) *height = 0u;
    return 0u;
}

void sms_backend_get_video_presentation(SmsBackendVideoPresentation *presentation)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->get_video_presentation != 0)
    {
        module->ops->get_video_presentation(presentation);
    }
}

unsigned sms_backend_audio_pull(int16_t *dst, unsigned max_samples)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->audio_pull != 0)
    {
        return module->ops->audio_pull(dst, max_samples);
    }
    if (dst != 0)
    {
        memset(dst, 0, max_samples * sizeof(dst[0]));
    }
    return max_samples;
}

void sms_backend_input_push_joystick(u8 bridge_bits)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->input_push_joystick != 0)
    {
        module->ops->input_push_joystick(bridge_bits);
    }
}

void sms_backend_input_push_keyboard(u8 modifiers, const unsigned char raw_keys[6])
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->input_push_keyboard != 0)
    {
        module->ops->input_push_keyboard(modifiers, raw_keys);
    }
}

boolean sms_backend_media_load_cartridge(unsigned slot_index, const char *path)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    boolean ok = FALSE;
    if (slot_index != 0u || path == 0 || path[0] == '\0')
    {
        return FALSE;
    }
    if (module != 0 && module->ops != 0 && module->ops->media_load_cartridge != 0)
    {
        ok = module->ops->media_load_cartridge(slot_index, path);
    }
    if (ok)
    {
        sms_backend_copy_text(g_sms_cart_name, sizeof(g_sms_cart_name), sms_backend_basename(path));
        g_sms_user_cartridge_loaded = TRUE;
    }
    return ok;
}

boolean sms_backend_media_load_disk(unsigned drive_index, const char *path)
{
    (void) drive_index;
    (void) path;
    return FALSE;
}

boolean sms_backend_media_load_cassette(const char *path)
{
    (void) path;
    return FALSE;
}

SmsBackendRuntimeStatus sms_backend_get_status(void)
{
    const BackendModuleV1 *module = g_sms_registry.selected_module;
    if (module != 0 && module->ops != 0 && module->ops->get_status != 0)
    {
        return module->ops->get_status();
    }
    SmsBackendRuntimeStatus status = { FALSE, FALSE, 0u, -1, SMS_BACKEND_INIT_ERR_CORE };
    return status;
}

SmsBackendInfo sms_backend_get_info(void)
{
    return g_sms_backend_info;
}

boolean sms_backend_reboot_basic(void) { return sms_backend_reset(); }
boolean sms_backend_menu_reset(void) { return sms_backend_reset(); }
boolean sms_backend_save_state(void *dst, unsigned capacity, unsigned *saved_size)
{
    (void) dst;
    (void) capacity;
    if (saved_size != 0) *saved_size = 0u;
    return FALSE;
}
boolean sms_backend_load_state(const void *src, unsigned size)
{
    (void) src;
    (void) size;
    return FALSE;
}

void sms_backend_set_overscan(boolean enabled) { (void) enabled; }
void sms_backend_set_refresh_hz(unsigned hz) { g_sms_refresh_hz = (hz == 50u) ? 50u : 60u; }
unsigned sms_backend_get_refresh_hz(void) { return g_sms_refresh_hz; }
uint64_t sms_backend_get_frame_interval_us_exact(void) { return g_sms_refresh_hz == 50u ? 20000ull : 16667ull; }
void sms_backend_set_disk_rom_enabled(boolean enabled) { (void) enabled; }
boolean sms_backend_get_disk_rom_enabled(void) { return FALSE; }
void sms_backend_set_color_artifacts_enabled(boolean enabled) { (void) enabled; }
boolean sms_backend_get_color_artifacts_enabled(void) { return FALSE; }
void sms_backend_set_gfx9000_enabled(boolean enabled) { (void) enabled; }
boolean sms_backend_get_gfx9000_enabled(void) { return FALSE; }
void sms_backend_set_boot_mode(unsigned mode) { (void) mode; }
unsigned sms_backend_get_boot_mode(void) { return 0u; }
unsigned sms_backend_get_active_boot_mode(void) { return 0u; }
void sms_backend_set_model_profile(unsigned profile) { (void) profile; }
unsigned sms_backend_get_model_profile(void) { return 0u; }
unsigned sms_backend_get_active_model_profile(void) { return 0u; }
void sms_backend_set_processor_mode(unsigned mode) { (void) mode; }
unsigned sms_backend_get_processor_mode(void) { return 0u; }
unsigned sms_backend_get_active_processor_mode(void) { return 0u; }
void sms_backend_set_rammapper_kb(unsigned kb) { (void) kb; }
unsigned sms_backend_get_rammapper_kb(void) { return 8u; }
void sms_backend_set_megaram_kb(unsigned kb) { (void) kb; }
unsigned sms_backend_get_megaram_kb(void) { return 0u; }
void sms_backend_set_z80_turbo_mode(unsigned mode) { (void) mode; }
unsigned sms_backend_get_z80_turbo_mode(void) { return 0u; }
void sms_backend_set_fm_music_enabled(boolean enabled)
{
    (void) enabled;
    smsplus_bare_set_fm_enabled(TRUE);
}
boolean sms_backend_get_fm_music_enabled(void) { return TRUE; }
void sms_backend_set_scc_plus_enabled(boolean enabled) { (void) enabled; }
boolean sms_backend_get_scc_plus_enabled(void) { return FALSE; }
boolean sms_backend_set_scc_dual_cart_enabled(boolean enabled) { (void) enabled; return FALSE; }
boolean sms_backend_get_scc_dual_cart_enabled(void) { return FALSE; }
boolean sms_backend_get_scc_dual_cart_available(void) { return FALSE; }
void sms_backend_set_fm_layer_gain_pct(unsigned pct) { (void) pct; }
unsigned sms_backend_get_fm_layer_gain_pct(void) { return 100u; }
void sms_backend_set_audio_layer_muted(unsigned layer_index, boolean muted)
{
    if (layer_index < 4u) g_sms_audio_layer_muted[layer_index] = muted;
}
boolean sms_backend_get_audio_layer_muted(unsigned layer_index)
{
    return layer_index < 4u ? g_sms_audio_layer_muted[layer_index] : TRUE;
}
unsigned sms_backend_get_psg_mixer_reg7(void) { return 0u; }
unsigned sms_backend_get_master_switch(void) { return 1u; }
void sms_backend_set_frame_perf_telemetry_enabled(boolean enabled) { (void) enabled; }
void sms_backend_reset_audio_mix_debug_window(void) {}

const char *sms_backend_get_cartridge_name(unsigned slot_index)
{
    return (slot_index == 0u && g_sms_user_cartridge_loaded) ? g_sms_cart_name : "";
}
const char *sms_backend_get_disk_name(unsigned drive_index) { (void) drive_index; return ""; }
const char *sms_backend_get_cassette_name(void) { return ""; }
boolean sms_backend_unload_cartridge(unsigned slot_index)
{
    if (slot_index != 0u) return FALSE;
    if (sms_backend_load_default_bios())
    {
        return TRUE;
    }
    g_sms_cart_name[0] = '\0';
    g_sms_user_cartridge_loaded = FALSE;
    return sms_backend_reset();
}
boolean sms_backend_unload_disk(unsigned drive_index) { (void) drive_index; return FALSE; }
boolean sms_backend_unload_cassette(void) { return FALSE; }

void sms_backend_reset_cheat_session(void) {}
boolean sms_backend_set_cheat_enabled(const TSmsCheatPoke *pokes, unsigned poke_count, boolean enabled)
{
    (void) pokes; (void) poke_count; (void) enabled; return FALSE;
}
boolean sms_backend_set_cheat_value(const TSmsCheatPoke *pokes, unsigned poke_count, uint16_t value)
{
    (void) pokes; (void) poke_count; (void) value; return FALSE;
}
boolean sms_backend_is_cheat_active(const TSmsCheatPoke *pokes, unsigned poke_count)
{
    (void) pokes; (void) poke_count; return FALSE;
}
boolean sms_backend_peek8(uint16_t address, uint8_t *value)
{
    (void) address;
    if (value != 0) *value = 0u;
    return FALSE;
}
boolean sms_backend_peek16(uint16_t address, uint16_t *value)
{
    (void) address;
    if (value != 0) *value = 0u;
    return FALSE;
}

void sms_backend_set_load_diag(unsigned stage) { g_sms_load_diag = stage; }
unsigned sms_backend_get_load_diag(void) { return g_sms_load_diag; }

#define SMS_ZERO_DEBUG(ptr) do { if ((ptr) != 0) memset((ptr), 0, sizeof(*(ptr))); } while (0)
void sms_backend_get_video_host_debug(SmsBackendVideoHostDebug *debug) { SMS_ZERO_DEBUG(debug); }
void sms_backend_get_audio_stats(SmsBackendAudioStats *stats) { smsplus_bare_fill_audio_stats(stats); }
void sms_backend_get_audio_mix_debug(SmsBackendAudioMixDebug *stats) { SMS_ZERO_DEBUG(stats); }
void sms_backend_get_audio_channel_debug(SmsBackendAudioChannelDebug *stats) { SMS_ZERO_DEBUG(stats); }
void sms_backend_get_keyboard_debug(SmsBackendKeyboardDebug *stats) { SMS_ZERO_DEBUG(stats); }
void sms_backend_get_timing_debug(SmsBackendTimingDebug *stats) { SMS_ZERO_DEBUG(stats); }
void sms_backend_get_vdp_trace_view(SmsBackendVdpTraceView *view) { SMS_ZERO_DEBUG(view); }
void sms_backend_get_audio_port_trace_view(SmsBackendAudioPortTraceView *view) { SMS_ZERO_DEBUG(view); }
void sms_backend_get_kmg_trace_view(SmsBackendKmgTraceView *view) { SMS_ZERO_DEBUG(view); }

unsigned sms_initramfs_mount(void)
{
    g_sms_initramfs_default_rom = 0;
    g_sms_initramfs_default_rom_size = 0u;
    g_sms_initramfs_files = 0u;
    g_sms_initramfs_source = "none";
    g_sms_initramfs_bytes = 0u;
    return sms_backend_mount_external_initramfs();
}
unsigned sms_initramfs_mount_boot_minimal(void) { return sms_initramfs_mount(); }
unsigned sms_initramfs_mount_remaining(void) { return 0u; }
unsigned sms_initramfs_files_mounted(void) { return g_sms_initramfs_files; }
const char *sms_initramfs_source(void) { return g_sms_initramfs_source; }
unsigned sms_initramfs_bytes(void) { return g_sms_initramfs_bytes; }

void sms_cheat_catalog_reset(TSmsCheatCatalog *catalog) { if (catalog != 0) memset(catalog, 0, sizeof(*catalog)); }
TSmsCheatLoadStatus sms_cheat_catalog_load(TSmsCheatCatalog *catalog, const char *path)
{
    (void) catalog; (void) path; return SmsCheatLoadFileOpenFailed;
}
TSmsCheatLoadStatus sms_cheat_catalog_load_default(TSmsCheatCatalog *catalog)
{
    (void) catalog; return SmsCheatLoadFileOpenFailed;
}
TSmsCheatLoadStatus sms_cheat_catalog_load_mcf(TSmsCheatCatalog *catalog, const char *path)
{
    (void) catalog; (void) path; return SmsCheatLoadFileOpenFailed;
}
const char *sms_cheat_catalog_status_name(TSmsCheatLoadStatus status)
{
    (void) status; return "unsupported";
}

static const TSmsMachineProfileSpec kSmsProfile = {
    0u,
    "sms",
    "SMS",
    "VDP",
    "NTSC 60Hz",
    "SN76489",
    "Cartridge ROM",
    0u,
    0u,
    60u,
    8u,
    0u,
    0u,
    0u,
    0u,
    0u
};

unsigned sms_machine_profile_count(void) { return 1u; }
unsigned sms_machine_profile_clamp(unsigned profile) { (void) profile; return 0u; }
const TSmsMachineProfileSpec *sms_machine_profile_get(unsigned profile)
{
    (void) profile;
    return &kSmsProfile;
}
const TSmsMachineProfileSpec *sms_machine_profile_default(void) { return &kSmsProfile; }
const char *sms_machine_profile_label(unsigned profile)
{
    (void) profile;
    return "SMS";
}
boolean sms_machine_profile_is_sms_family(unsigned profile)
{
    (void) profile;
    return FALSE;
}
