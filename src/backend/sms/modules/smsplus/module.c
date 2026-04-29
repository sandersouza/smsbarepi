#include "../../backend.h"

#include "../../../../../third_party/smsplus/shared.h"

#include <fatfs/ff.h>
#include <string.h>

enum {
    SmsPlusFrameWidth = 256u,
    SmsPlusFrameHeight = 240u,
    SmsPlusMaxRomBytes = 1u * 1024u * 1024u
};

static BackendRuntimeStatus g_smsplus_status = {
    FALSE, FALSE, 0u, 0, SMS_BACKEND_INIT_OK
};
static BackendHostExportsV1 g_smsplus_host;
static uint8 g_smsplus_indexed_frame[SmsPlusFrameWidth * SmsPlusFrameHeight];
static u16 g_smsplus_rgb565_frame[SmsPlusFrameWidth * SmsPlusFrameHeight];
static uint8 g_smsplus_rom[SmsPlusMaxRomBytes];
static uint8 g_smsplus_dummy_rom[0x4000];
static unsigned g_smsplus_sequence = 0u;
static boolean g_smsplus_core_initialized = FALSE;
static boolean g_smsplus_rom_loaded = FALSE;
static boolean g_smsplus_diagnostic_logged = FALSE;

extern unsigned smsplus_bare_audio_pull(int16_t *dst, unsigned max_samples);
extern void smsplus_bare_sound_configure(void);
extern void smsplus_bare_audio_frame_ready(void);
extern void smsplus_bare_audio_reset_buffer(void);

static void smsplus_apply_fm_setting(void)
{
    sms.use_fm = 1;
    sms.console = CONSOLE_SMSJ;
    sms.territory = TERRITORY_DOMESTIC;
    sms.display = DISPLAY_NTSC;
}

static void smsplus_apply_configured_fm(void)
{
    smsplus_apply_fm_setting();
}

void smsplus_bare_set_fm_enabled(boolean enabled)
{
    (void) enabled;
    const boolean changed =
        (sms.use_fm == 0)
        || (sms.console != CONSOLE_SMSJ)
        || (sms.territory != TERRITORY_DOMESTIC);
    smsplus_apply_fm_setting();
    if (g_smsplus_core_initialized && changed)
    {
        smsplus_bare_audio_reset_buffer();
        system_reinit();
        system_reset();
    }
}

static void smsplus_log(const char *text)
{
    if (g_smsplus_host.uart_write != 0 && text != 0)
    {
        g_smsplus_host.uart_write(text);
        g_smsplus_host.uart_write("\n");
    }
}

static void smsplus_prepare_bitmap(void)
{
    bitmap.data = g_smsplus_indexed_frame;
    bitmap.width = SmsPlusFrameWidth;
    bitmap.height = SmsPlusFrameHeight;
    bitmap.pitch = SmsPlusFrameWidth;
    bitmap.depth = 8;
    bitmap.granularity = 1;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 192;
    bitmap.viewport.ox = 0;
    bitmap.viewport.oy = 0;
    bitmap.viewport.ow = 256;
    bitmap.viewport.oh = 192;
    bitmap.viewport.changed = 1;
}

static void smsplus_install_dummy_rom(void)
{
    memset(g_smsplus_dummy_rom, 0, sizeof(g_smsplus_dummy_rom));
    cart.rom = g_smsplus_dummy_rom;
    cart.pages = 1u;
    cart.crc = 0u;
    cart.sram_crc = 0u;
    cart.mapper = MAPPER_SEGA;
    memset(cart.sram, 0, sizeof(cart.sram));
    memset(cart.fcr, 0, sizeof(cart.fcr));
    smsplus_apply_configured_fm();
    g_smsplus_rom_loaded = FALSE;
}

static void smsplus_draw_no_rom_diagnostic_frame(void)
{
    static const u16 bars[8] = {
        0x001Fu, 0x07E0u, 0xF800u, 0xFFE0u,
        0xF81Fu, 0x07FFu, 0xFFFFu, 0x8410u
    };
    for (unsigned y = 0u; y < SmsPlusFrameHeight; ++y)
    {
        u16 *dst = g_smsplus_rgb565_frame + (y * SmsPlusFrameWidth);
        for (unsigned x = 0u; x < SmsPlusFrameWidth; ++x)
        {
            const unsigned bar = (x * 8u) / SmsPlusFrameWidth;
            u16 color = bars[bar < 8u ? bar : 7u];
            if (y < 16u || y >= 176u || x < 16u || x >= 240u)
            {
                color = 0x39E7u;
            }
            if (((x ^ y) & 0x20u) != 0u)
            {
                color = (u16) ((color & 0xF7DEu) >> 1);
            }
            dst[x] = color;
        }
    }
}

static boolean smsplus_load_rom_bytes(const char *path, unsigned *out_size)
{
    FIL file;
    FRESULT fr;
    UINT got = 0u;
    UINT total = 0u;
    FSIZE_t size;

    if (out_size != 0)
    {
        *out_size = 0u;
    }
    if (path == 0 || path[0] == '\0')
    {
        return FALSE;
    }

    fr = f_open(&file, path, FA_READ);
    if (fr != FR_OK)
    {
        return FALSE;
    }

    size = f_size(&file);
    if (size > SmsPlusMaxRomBytes || size < 0x2000u)
    {
        f_close(&file);
        return FALSE;
    }

    while (total < (UINT) size)
    {
        UINT request = (UINT) size - total;
        if (request > 32768u)
        {
            request = 32768u;
        }
        fr = f_read(&file, g_smsplus_rom + total, request, &got);
        if (fr != FR_OK || got == 0u)
        {
            f_close(&file);
            return FALSE;
        }
        total += got;
    }
    f_close(&file);

    if ((total / 512u) & 1u)
    {
        memmove(g_smsplus_rom, g_smsplus_rom + 512u, total - 512u);
        total -= 512u;
    }
    if (total < 0x2000u)
    {
        return FALSE;
    }
    if (total < 0x4000u)
    {
        memset(g_smsplus_rom + total, 0xFF, 0x4000u - total);
        total = 0x4000u;
    }
    if (out_size != 0)
    {
        *out_size = total;
    }
    return TRUE;
}

static void smsplus_configure_loaded_rom(const char *path, unsigned size)
{
    cart.rom = g_smsplus_rom;
    cart.pages = (uint8) (size / 0x4000u);
    if (cart.pages == 0u)
    {
        cart.pages = 1u;
    }
    cart.crc = 0u;
    cart.sram_crc = 0u;
    cart.mapper = MAPPER_SEGA;
    memset(cart.sram, 0, sizeof(cart.sram));
    memset(cart.fcr, 0, sizeof(cart.fcr));

    (void) path;
    smsplus_apply_configured_fm();
    system_assign_device(PORT_A, DEVICE_PAD2B);
    system_assign_device(PORT_B, DEVICE_PAD2B);
    g_smsplus_rom_loaded = TRUE;
}

boolean smsplus_media_load_cartridge_bytes(const char *label, const uint8 *data, unsigned size)
{
    if (data == 0 || size > SmsPlusMaxRomBytes || size < 0x2000u)
    {
        return FALSE;
    }

    memcpy(g_smsplus_rom, data, size);
    if ((size / 512u) & 1u)
    {
        memmove(g_smsplus_rom, g_smsplus_rom + 512u, size - 512u);
        size -= 512u;
    }
    if (size < 0x4000u)
    {
        memset(g_smsplus_rom + size, 0xFF, 0x4000u - size);
        size = 0x4000u;
    }
    smsplus_configure_loaded_rom(label, size);
    if (g_smsplus_core_initialized)
    {
        smsplus_bare_audio_reset_buffer();
        system_reinit();
        system_reset();
    }
    return TRUE;
}

static void smsplus_convert_frame(void)
{
    unsigned src_y = (unsigned) bitmap.viewport.y;
    unsigned src_h = (unsigned) bitmap.viewport.h;
    if (src_h == 0u || src_h > SmsPlusFrameHeight)
    {
        src_h = 192u;
    }
    if (src_y + src_h > SmsPlusFrameHeight)
    {
        src_y = 0u;
    }

    for (unsigned y = 0u; y < SmsPlusFrameHeight; ++y)
    {
        u16 *dst = g_smsplus_rgb565_frame + (y * SmsPlusFrameWidth);
        if (y >= src_h)
        {
            memset(dst, 0, SmsPlusFrameWidth * sizeof(dst[0]));
            continue;
        }
        const uint8 *src = g_smsplus_indexed_frame + ((src_y + y) * SmsPlusFrameWidth);
        for (unsigned x = 0u; x < SmsPlusFrameWidth; ++x)
        {
            dst[x] = pixel[src[x] & PIXEL_MASK];
        }
    }
}

static boolean smsplus_init(const BackendHostExportsV1 *host, int *out_error_code)
{
    if (host != 0)
    {
        g_smsplus_host = *host;
    }
    if (out_error_code != 0)
    {
        *out_error_code = SMS_BACKEND_INIT_OK;
    }

    smsplus_prepare_bitmap();
    smsplus_install_dummy_rom();
    smsplus_bare_sound_configure();
    smsplus_draw_no_rom_diagnostic_frame();
    system_init();
    g_smsplus_core_initialized = TRUE;

    g_smsplus_status.initialized = TRUE;
    g_smsplus_status.running = TRUE;
    g_smsplus_status.frames = 0u;
    g_smsplus_status.last_pc = 0;
    g_smsplus_status.last_error = SMS_BACKEND_INIT_OK;
    smsplus_log("SMS Plus GX backend initialized");
    return TRUE;
}

static boolean smsplus_reset(void)
{
    if (!g_smsplus_core_initialized)
    {
        return FALSE;
    }
    smsplus_bare_audio_reset_buffer();
    system_reset();
    g_smsplus_status.running = TRUE;
    return TRUE;
}

static boolean smsplus_step_frame(void)
{
    if (!g_smsplus_core_initialized)
    {
        return FALSE;
    }
    system_frame(0);
    smsplus_bare_audio_frame_ready();
    if (g_smsplus_rom_loaded)
    {
        smsplus_convert_frame();
    }
    else
    {
        smsplus_draw_no_rom_diagnostic_frame();
        if (!g_smsplus_diagnostic_logged)
        {
            smsplus_log("SMS Plus GX diagnostic frame active: no .sms ROM loaded");
            g_smsplus_diagnostic_logged = TRUE;
        }
    }
    ++g_smsplus_sequence;
    ++g_smsplus_status.frames;
    g_smsplus_status.last_pc = z80_get_reg(Z80_PC);
    return TRUE;
}

static void smsplus_shutdown(void)
{
    if (g_smsplus_core_initialized)
    {
        system_shutdown();
    }
    g_smsplus_core_initialized = FALSE;
    g_smsplus_status.running = FALSE;
}

static unsigned smsplus_video_frame(const u16 **pixels_out, unsigned *width, unsigned *height)
{
    if (pixels_out != 0)
    {
        *pixels_out = g_smsplus_rgb565_frame;
    }
    if (width != 0)
    {
        *width = SmsPlusFrameWidth;
    }
    if (height != 0)
    {
        *height = (unsigned) bitmap.viewport.h;
        if (*height == 0u || *height > SmsPlusFrameHeight)
        {
            *height = 192u;
        }
    }
    return g_smsplus_sequence;
}

static void smsplus_get_video_presentation(BackendVideoPresentation *presentation)
{
    if (presentation == 0)
    {
        return;
    }
    memset(presentation, 0, sizeof(*presentation));
    presentation->logical_width = SmsPlusFrameWidth;
    presentation->logical_height = (unsigned) bitmap.viewport.h;
    if (presentation->logical_height == 0u || presentation->logical_height > SmsPlusFrameHeight)
    {
        presentation->logical_height = 192u;
    }
    presentation->pitch_pixels = SmsPlusFrameWidth;
    presentation->source_width = SmsPlusFrameWidth;
    presentation->source_height = presentation->logical_height;
    presentation->disable_scanline_postfx = FALSE;
}

static unsigned smsplus_audio_pull(int16_t *dst, unsigned max_samples)
{
    return smsplus_bare_audio_pull(dst, max_samples);
}

static void smsplus_input_push_joystick(u8 bridge_bits)
{
    uint32 pad = 0u;
    if ((bridge_bits & (1u << 0)) != 0u) pad |= INPUT_RIGHT;
    if ((bridge_bits & (1u << 1)) != 0u) pad |= INPUT_LEFT;
    if ((bridge_bits & (1u << 2)) != 0u) pad |= INPUT_DOWN;
    if ((bridge_bits & (1u << 3)) != 0u) pad |= INPUT_UP;
    if ((bridge_bits & (1u << 4)) != 0u) pad |= INPUT_BUTTON1;
    if ((bridge_bits & (1u << 5)) != 0u) pad |= INPUT_BUTTON2;
    input.pad[0] = pad;
}

static void smsplus_input_push_keyboard(u8 modifiers, const unsigned char raw_keys[6])
{
    (void) modifiers;
    (void) raw_keys;
}

static boolean smsplus_media_load_cartridge(unsigned slot_index, const char *path)
{
    unsigned size = 0u;
    if (slot_index != 0u || !smsplus_load_rom_bytes(path, &size))
    {
        return FALSE;
    }
    smsplus_configure_loaded_rom(path, size);
    if (g_smsplus_core_initialized)
    {
        smsplus_bare_audio_reset_buffer();
        system_reinit();
        system_reset();
    }
    return TRUE;
}

static BackendRuntimeStatus smsplus_get_status(void)
{
    return g_smsplus_status;
}

static const BackendOps kSmsPlusOps = {
    smsplus_init,
    smsplus_reset,
    smsplus_step_frame,
    smsplus_shutdown,
    smsplus_video_frame,
    smsplus_get_video_presentation,
    smsplus_audio_pull,
    smsplus_input_push_joystick,
    smsplus_input_push_keyboard,
    smsplus_media_load_cartridge,
    0,
    0,
    smsplus_get_status
};

const BackendModuleV1 g_sms_backend_module_smsplus = {
    BACKEND_MODULE_ABI_V1,
    "sms",
    "sms.smsplus",
    "SMS Plus GX",
    SMS_BACKEND_CAP_STATE | SMS_BACKEND_CAP_MACHINE | SMS_BACKEND_CAP_MEDIA | SMS_BACKEND_CAP_DEBUG,
    &kSmsPlusOps,
    0
};
