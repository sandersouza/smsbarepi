#include "viewport_screenshot.h"

#ifndef SMSBARE_VIEWPORT_SCREENSHOT_MAX_PIXELS
#define SMSBARE_VIEWPORT_SCREENSHOT_MAX_PIXELS (1920u * 1080u)
#endif

static TScreenColor *g_ViewportScreenshotBuffer = 0;
static unsigned g_ViewportScreenshotX = 0u;
static unsigned g_ViewportScreenshotY = 0u;
static unsigned g_ViewportScreenshotW = 0u;
static unsigned g_ViewportScreenshotH = 0u;
static boolean g_ViewportScreenshotValid = FALSE;
static boolean g_ViewportScreenshotAllocTried = FALSE;

#if DEPTH == 32
static TScreenColor ConvertRgb565ToScreenColor(u16 v)
{
    const unsigned r5 = (unsigned) ((v >> 11) & 0x1Fu);
    const unsigned g6 = (unsigned) ((v >> 5) & 0x3Fu);
    const unsigned b5 = (unsigned) (v & 0x1Fu);
    const unsigned r8 = (r5 * 255u) / 31u;
    const unsigned g8 = (g6 * 255u) / 63u;
    const unsigned b8 = (b5 * 255u) / 31u;
    return COLOR32(r8, g8, b8, 0xFFu);
}
#else
static TScreenColor ConvertRgb565ToScreenColor(u16 v)
{
    return (TScreenColor) v;
}
#endif

static boolean EnsureScreenshotBuffer(void)
{
    if (g_ViewportScreenshotBuffer != 0)
    {
        return TRUE;
    }
    if (!g_ViewportScreenshotAllocTried)
    {
        g_ViewportScreenshotAllocTried = TRUE;
        g_ViewportScreenshotBuffer = new TScreenColor[SMSBARE_VIEWPORT_SCREENSHOT_MAX_PIXELS];
    }
    return (g_ViewportScreenshotBuffer != 0) ? TRUE : FALSE;
}

static u16 DimRgb565Half(u16 sample)
{
    const u16 rgb = (u16) (sample & 0xF7DEu);
    return (u16) (rgb >> 1);
}

boolean combo_viewport_screenshot_capture(CGfxTextBoxRenderer *renderer,
                                          unsigned x,
                                          unsigned y,
                                          unsigned w,
                                          unsigned h)
{
    if (renderer == 0 || w == 0u || h == 0u)
    {
        g_ViewportScreenshotValid = FALSE;
        return FALSE;
    }

    const unsigned need_pixels = w * h;
    if (need_pixels == 0u || need_pixels > SMSBARE_VIEWPORT_SCREENSHOT_MAX_PIXELS)
    {
        g_ViewportScreenshotValid = FALSE;
        return FALSE;
    }

    if (!EnsureScreenshotBuffer())
    {
        g_ViewportScreenshotValid = FALSE;
        return FALSE;
    }

    TGfxOffscreenState offscreen_state;
    renderer->SaveOffscreenState(&offscreen_state);
    if (offscreen_state.active)
    {
        renderer->CancelOffscreen();
    }

    renderer->CopyRectFromScreen(x, y, w, h, g_ViewportScreenshotBuffer, w);

    if (offscreen_state.active)
    {
        renderer->RestoreOffscreenState(&offscreen_state);
    }

    g_ViewportScreenshotX = x;
    g_ViewportScreenshotY = y;
    g_ViewportScreenshotW = w;
    g_ViewportScreenshotH = h;
    g_ViewportScreenshotValid = TRUE;
    return TRUE;
}

boolean combo_viewport_screenshot_capture_from_emulator(const u16 *src_pixels,
                                                        unsigned src_w,
                                                        unsigned src_h,
                                                        unsigned src_pitch_pixels,
                                                        unsigned dst_x,
                                                        unsigned dst_y,
                                                        unsigned dst_w,
                                                        unsigned dst_h,
                                                        unsigned full_dst_w,
                                                        unsigned full_dst_h,
                                                        unsigned crop_x,
                                                        unsigned crop_y,
                                                        unsigned scanline_mode)
{
    if (src_pixels == 0 || src_w == 0u || src_h == 0u || dst_w == 0u || dst_h == 0u)
    {
        g_ViewportScreenshotValid = FALSE;
        return FALSE;
    }

    const unsigned need_pixels = dst_w * dst_h;
    if (need_pixels == 0u || need_pixels > SMSBARE_VIEWPORT_SCREENSHOT_MAX_PIXELS)
    {
        g_ViewportScreenshotValid = FALSE;
        return FALSE;
    }
    if (!EnsureScreenshotBuffer())
    {
        g_ViewportScreenshotValid = FALSE;
        return FALSE;
    }

    if (full_dst_w == 0u)
    {
        full_dst_w = dst_w;
    }
    if (full_dst_h == 0u)
    {
        full_dst_h = dst_h;
    }
    if (crop_x >= full_dst_w)
    {
        crop_x = 0u;
    }
    if (crop_y >= full_dst_h)
    {
        crop_y = 0u;
    }
    if (src_pitch_pixels == 0u)
    {
        src_pitch_pixels = src_w;
    }

    for (unsigned y = 0u; y < dst_h; ++y)
    {
        const unsigned src_y =
            (unsigned) (((u64) (y + crop_y) * (u64) src_h) / (u64) full_dst_h);
        const u16 *src_row = src_pixels + (src_y * src_pitch_pixels);
        TScreenColor *dst_row = g_ViewportScreenshotBuffer + (y * dst_w);
        const boolean dark_lcd = (scanline_mode == 1u) && ((y & 1u) != 0u);
        for (unsigned x = 0u; x < dst_w; ++x)
        {
            const unsigned src_x =
                (unsigned) (((u64) (x + crop_x) * (u64) src_w) / (u64) full_dst_w);
            u16 sample = src_row[src_x];
            if (dark_lcd)
            {
                sample = DimRgb565Half(sample);
            }
            dst_row[x] = ConvertRgb565ToScreenColor(sample);
        }
    }

    g_ViewportScreenshotX = dst_x;
    g_ViewportScreenshotY = dst_y;
    g_ViewportScreenshotW = dst_w;
    g_ViewportScreenshotH = dst_h;
    g_ViewportScreenshotValid = TRUE;
    return TRUE;
}

boolean combo_viewport_screenshot_get(TViewportScreenshotView *out_view)
{
    if (out_view == 0)
    {
        return FALSE;
    }

    out_view->pixels = 0;
    out_view->x = 0u;
    out_view->y = 0u;
    out_view->w = 0u;
    out_view->h = 0u;
    out_view->stride = 0u;
    out_view->address = (uintptr) g_ViewportScreenshotBuffer;

    if (g_ViewportScreenshotBuffer == 0 || !g_ViewportScreenshotValid || g_ViewportScreenshotW == 0u || g_ViewportScreenshotH == 0u)
    {
        return FALSE;
    }

    out_view->pixels = g_ViewportScreenshotBuffer;
    out_view->x = g_ViewportScreenshotX;
    out_view->y = g_ViewportScreenshotY;
    out_view->w = g_ViewportScreenshotW;
    out_view->h = g_ViewportScreenshotH;
    out_view->stride = g_ViewportScreenshotW;
    return TRUE;
}

uintptr combo_viewport_screenshot_address(void)
{
    return (uintptr) g_ViewportScreenshotBuffer;
}
