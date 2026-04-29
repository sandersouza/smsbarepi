#ifndef SMSBARE_FRAMEBUFFER_H
#define SMSBARE_FRAMEBUFFER_H

#include <stdint.h>

typedef struct framebuffer_info {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t is_rgb;
    uint8_t *pixels;
    uint32_t size;
} framebuffer_info_t;

int framebuffer_init(framebuffer_info_t *fb, uint32_t width, uint32_t height);
void framebuffer_fill(framebuffer_info_t *fb, uint32_t color);
void framebuffer_draw_rect(framebuffer_info_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

#endif