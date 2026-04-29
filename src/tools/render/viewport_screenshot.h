#ifndef SMSBARE_CIRCLE_VIEWPORT_SCREENSHOT_H
#define SMSBARE_CIRCLE_VIEWPORT_SCREENSHOT_H

#include "gfx_textbox.h"

struct TViewportScreenshotView
{
    const TScreenColor *pixels;
    unsigned x;
    unsigned y;
    unsigned w;
    unsigned h;
    unsigned stride;
    uintptr address;
};

boolean combo_viewport_screenshot_capture(CGfxTextBoxRenderer *renderer,
                                          unsigned x,
                                          unsigned y,
                                          unsigned w,
                                          unsigned h);
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
                                                        unsigned scanline_mode);
boolean combo_viewport_screenshot_get(TViewportScreenshotView *out_view);
uintptr combo_viewport_screenshot_address(void);

#endif
