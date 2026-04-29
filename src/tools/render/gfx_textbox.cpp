#include "gfx_textbox.h"

#include <circle/types.h>

static const unsigned char kFolderGlyphLeftChar = 0x1Eu;
static const unsigned char kFolderGlyphRightChar = 0x1Fu;
static const unsigned char kArrowUpGlyphChar = 0x1Cu;
static const unsigned char kArrowDownGlyphChar = 0x1Du;
static const unsigned char kBlockFullGlyphChar = 0x1Bu;

/* Custom 8x8 matrices (1=foreground pixel, 0=transparent/background). */
static const unsigned char kArrowUpMatrix8x8[8] = {
    0x00u, /* 00000000 */
    0x00u, /* 00000000 */
    0x00u, /* 00000000 */
    0x00u, /* 00000000 */
    0x10u, /* 00010000 */
    0x38u, /* 00111000 */
    0x7Cu, /* 01111100 */
    0x00u  /* 00000000 */
};

static const unsigned char kArrowDownMatrix8x8[8] = {
    0x00u, /* 00000000 */
    0x7Cu, /* 01111100 */
    0x38u, /* 00111000 */
    0x10u, /* 00010000 */
    0x00u, /* 00000000 */
    0x00u, /* 00000000 */
    0x00u, /* 00000000 */
    0x00u  /* 00000000 */
};

static const unsigned char kBlockMatrix8x8[8] = {
    0xFEu, /* 11111110 */
    0xFEu, /* 11111110 */
    0xFEu, /* 11111110 */
    0xFEu, /* 11111110 */
    0xFEu, /* 11111110 */
    0xFEu, /* 11111110 */
    0xFEu, /* 11111110 */
    0xFEu  /* 11111110 */
};

static unsigned DecodeUtf8Latin1(const char *text, unsigned *consumed)
{
    if (consumed == 0 || text == 0 || text[0] == '\0')
    {
        if (consumed != 0) *consumed = 0u;
        return 0u;
    }

    const unsigned b0 = (unsigned) (unsigned char) text[0];
    if (b0 < 0x80u)
    {
        *consumed = 1u;
        return b0;
    }

    const unsigned b1 = (unsigned) (unsigned char) text[1];
    if ((b0 == 0xC2u || b0 == 0xC3u) && (b1 >= 0x80u && b1 <= 0xBFu))
    {
        *consumed = 2u;
        return ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
    }

    *consumed = 1u;
    return '?';
}

static void DrawCustomFolderGlyphHalf(CGfxTextBoxRenderer *renderer,
                                      unsigned x,
                                      unsigned y,
                                      const TGfxTextStyle &style,
                                      unsigned scale_x,
                                      unsigned scale_y,
                                      unsigned char_w,
                                      unsigned char_h,
                                      boolean right_half)
{
    if (renderer == 0 || char_w == 0u || char_h == 0u)
    {
        return;
    }

    if (style.opaque_bg)
    {
        renderer->FillRect(x, y, char_w * scale_x, char_h * scale_y, style.bg);
    }

    const unsigned full_w = char_w * 2u;
    const unsigned half_offset = right_half ? char_w : 0u;
    const unsigned body_bottom = (char_h > 5u) ? (char_h - 5u) : (char_h - 1u);

    auto put_scaled = [&](unsigned px, unsigned py) {
        renderer->FillRect(x + (px * scale_x), y + (py * scale_y), scale_x, scale_y, style.fg);
    };

    for (unsigned lx = 0u; lx < char_w; ++lx)
    {
        const unsigned gx = half_offset + lx; /* 0..(2*char_w-1) */
        if (gx >= full_w)
        {
            continue;
        }

        /* Folder silhouette close to the reference:
         * - tab on top-left
         * - slight step from tab to main top edge
         * - filled body */
        unsigned top = char_h; /* default: no pixel */
        if (gx >= 1u && gx <= 5u)
        {
            top = 2u; /* tab top */
        }
        else if (gx == 6u || gx == 7u)
        {
            top = 3u; /* transition */
        }
        else if (gx >= 8u && gx + 1u < full_w)
        {
            top = 4u; /* main top */
        }

        if (top >= char_h)
        {
            continue;
        }

        if (body_bottom >= top)
        {
            for (unsigned py = top; py <= body_bottom; ++py)
            {
                put_scaled(lx, py);
            }
        }
    }
}

static void DrawGlyphMatrix8x8(CGfxTextBoxRenderer *renderer,
                               unsigned x,
                               unsigned y,
                               const TGfxTextStyle &style,
                               unsigned /*scale_x*/,
                               unsigned /*scale_y*/,
                               unsigned /*char_w*/,
                               unsigned /*char_h*/,
                               const unsigned char rows[8])
{
    if (renderer == 0 || rows == 0)
    {
        return;
    }

    const unsigned target_w = 16u;
    const unsigned target_h = 32u;
    if (style.opaque_bg)
    {
        renderer->FillRect(x, y, target_w, target_h, style.bg);
    }

    for (unsigned gy = 0u; gy < 8u; ++gy)
    {
        const unsigned y0 = gy * 4u;
        const unsigned h = 4u;
        const unsigned char row = rows[gy];
        for (unsigned gx = 0u; gx < 8u; ++gx)
        {
            const unsigned mask = (unsigned) (1u << (7u - gx));
            if ((row & mask) == 0u)
            {
                continue;
            }
            const unsigned x0 = gx * 2u;
            const unsigned w = 2u;
            renderer->FillRect(x + x0, y + y0, w, h, style.fg);
        }
    }
}

static void DrawCustomArrowUpGlyph(CGfxTextBoxRenderer *renderer,
                                   unsigned x,
                                   unsigned y,
                                   const TGfxTextStyle &style,
                                   unsigned scale_x,
                                   unsigned scale_y,
                                   unsigned char_w,
                                   unsigned char_h)
{
    DrawGlyphMatrix8x8(renderer, x, y, style, scale_x, scale_y, char_w, char_h, kArrowUpMatrix8x8);
}

static void DrawCustomFullBlockGlyph(CGfxTextBoxRenderer *renderer,
                                     unsigned x,
                                     unsigned y,
                                     const TGfxTextStyle &style,
                                     unsigned scale_x,
                                     unsigned scale_y,
                                     unsigned char_w,
                                     unsigned char_h)
{
    DrawGlyphMatrix8x8(renderer, x, y, style, scale_x, scale_y, char_w, char_h, kBlockMatrix8x8);
}

static void DrawCustomArrowDownGlyph(CGfxTextBoxRenderer *renderer,
                                     unsigned x,
                                     unsigned y,
                                     const TGfxTextStyle &style,
                                     unsigned scale_x,
                                     unsigned scale_y,
                                     unsigned char_w,
                                     unsigned char_h)
{
    DrawGlyphMatrix8x8(renderer, x, y, style, scale_x, scale_y, char_w, char_h, kArrowDownMatrix8x8);
}

CGfxTextBoxRenderer::CGfxTextBoxRenderer(CComboScreenDevice *screen)
: m_Screen(0)
, m_FrameBuffer(0)
, m_Font(DEFAULT_FONT, CCharGenerator::FontFlagsNone)
, m_OffscreenActive(FALSE)
, m_OffscreenOriginX(0u)
, m_OffscreenOriginY(0u)
, m_OffscreenW(0u)
, m_OffscreenH(0u)
, m_OffscreenStride(0u)
, m_OffscreenBuffer(0)
{
	Attach(screen);
}

void CGfxTextBoxRenderer::Attach(CComboScreenDevice *screen)
{
	m_Screen = screen;
	m_FrameBuffer = 0;
	if (m_Screen != 0)
	{
		m_FrameBuffer = m_Screen->GetFrameBuffer();
	}
	m_OffscreenActive = FALSE;
	m_OffscreenOriginX = 0u;
	m_OffscreenOriginY = 0u;
	m_OffscreenW = 0u;
	m_OffscreenH = 0u;
	m_OffscreenStride = 0u;
	m_OffscreenBuffer = 0;
}

unsigned CGfxTextBoxRenderer::GetCharWidth(void) const
{
	return m_Font.GetCharWidth();
}

unsigned CGfxTextBoxRenderer::GetCharHeight(void) const
{
	return m_Font.GetCharHeight();
}

void CGfxTextBoxRenderer::PutPixel(unsigned x, unsigned y, TScreenColor color)
{
	if (m_Screen == 0)
	{
		return;
	}

	if (m_OffscreenActive)
	{
		if (m_OffscreenBuffer == 0 || m_OffscreenW == 0u || m_OffscreenH == 0u || m_OffscreenStride == 0u)
		{
			return;
		}
		if (x < m_OffscreenOriginX || y < m_OffscreenOriginY)
		{
			return;
		}
		const unsigned local_x = x - m_OffscreenOriginX;
		const unsigned local_y = y - m_OffscreenOriginY;
		if (local_x >= m_OffscreenW || local_y >= m_OffscreenH)
		{
			return;
		}
		m_OffscreenBuffer[(local_y * m_OffscreenStride) + local_x] = color;
		return;
	}

	const unsigned screen_w = m_Screen->GetWidth();
	const unsigned screen_h = m_Screen->GetHeight();
	if (x >= screen_w || y >= screen_h)
	{
		return;
	}

	if (m_FrameBuffer != 0 && m_FrameBuffer->GetDepth() == 16)
	{
		u16 *base = m_Screen->GetDrawBuffer16();
		const unsigned pitch_pixels = m_Screen->GetPitchPixels();
		base[(y * pitch_pixels) + x] = (u16) color;
		m_Screen->MarkDirtyRect(x, y, 1u, 1u);
		return;
	}

	m_Screen->SetPixel(x, y, color);
}

void CGfxTextBoxRenderer::BeginOffscreen(unsigned origin_x, unsigned origin_y, unsigned w, unsigned h,
					 TScreenColor *buffer, unsigned buffer_stride_pixels)
{
	if (buffer == 0 || w == 0u || h == 0u || buffer_stride_pixels < w)
	{
		CancelOffscreen();
		return;
	}
	m_OffscreenActive = TRUE;
	m_OffscreenOriginX = origin_x;
	m_OffscreenOriginY = origin_y;
	m_OffscreenW = w;
	m_OffscreenH = h;
	m_OffscreenStride = buffer_stride_pixels;
	m_OffscreenBuffer = buffer;
}

void CGfxTextBoxRenderer::CopyRectFromScreen(unsigned x, unsigned y, unsigned w, unsigned h,
					     TScreenColor *buffer, unsigned buffer_stride_pixels)
{
	if (m_Screen == 0 || buffer == 0 || w == 0u || h == 0u || buffer_stride_pixels < w)
	{
		return;
	}

	const unsigned screen_w = m_Screen->GetWidth();
	const unsigned screen_h = m_Screen->GetHeight();
	if (x >= screen_w || y >= screen_h)
	{
		return;
	}
	const unsigned max_w = (x + w <= screen_w) ? w : (screen_w - x);
	const unsigned max_h = (y + h <= screen_h) ? h : (screen_h - y);

	if (m_FrameBuffer != 0 && m_FrameBuffer->GetDepth() == 16)
	{
		const u16 *src_base = m_Screen->GetDrawBuffer16();
		const unsigned src_pitch = m_Screen->GetPitchPixels();
		for (unsigned row = 0u; row < max_h; ++row)
		{
			const u16 *src = src_base + ((y + row) * src_pitch) + x;
			TScreenColor *dst = buffer + (row * buffer_stride_pixels);
			for (unsigned col = 0u; col < max_w; ++col)
			{
				dst[col] = (TScreenColor) src[col];
			}
		}
		return;
	}

	for (unsigned row = 0u; row < max_h; ++row)
	{
		TScreenColor *dst = buffer + (row * buffer_stride_pixels);
		for (unsigned col = 0u; col < max_w; ++col)
		{
			dst[col] = m_Screen->GetPixel(x + col, y + row);
		}
	}
}

void CGfxTextBoxRenderer::CopyRectFromVisibleScreen(unsigned x, unsigned y, unsigned w, unsigned h,
						    TScreenColor *buffer, unsigned buffer_stride_pixels)
{
	if (m_Screen == 0 || buffer == 0 || w == 0u || h == 0u || buffer_stride_pixels < w)
	{
		return;
	}

	const unsigned screen_w = m_Screen->GetWidth();
	const unsigned screen_h = m_Screen->GetHeight();
	if (x >= screen_w || y >= screen_h)
	{
		return;
	}
	const unsigned max_w = (x + w <= screen_w) ? w : (screen_w - x);
	const unsigned max_h = (y + h <= screen_h) ? h : (screen_h - y);

	if (m_FrameBuffer != 0 && m_FrameBuffer->GetDepth() == 16)
	{
		const u16 *src_base = m_Screen->GetVisibleBuffer16();
		const unsigned src_pitch = m_Screen->GetPitchPixels();
		for (unsigned row = 0u; row < max_h; ++row)
		{
			const u16 *src = src_base + ((y + row) * src_pitch) + x;
			TScreenColor *dst = buffer + (row * buffer_stride_pixels);
			for (unsigned col = 0u; col < max_w; ++col)
			{
				dst[col] = (TScreenColor) src[col];
			}
		}
		return;
	}

	for (unsigned row = 0u; row < max_h; ++row)
	{
		TScreenColor *dst = buffer + (row * buffer_stride_pixels);
		for (unsigned col = 0u; col < max_w; ++col)
		{
			dst[col] = m_Screen->GetPixel(x + col, y + row);
		}
	}
}

void CGfxTextBoxRenderer::EndOffscreenBlit(void)
{
	if (!m_OffscreenActive || m_Screen == 0 || m_OffscreenBuffer == 0)
	{
		CancelOffscreen();
		return;
	}

	const unsigned origin_x = m_OffscreenOriginX;
	const unsigned origin_y = m_OffscreenOriginY;
	const unsigned w = m_OffscreenW;
	const unsigned h = m_OffscreenH;
	const unsigned stride = m_OffscreenStride;
	TScreenColor *src = m_OffscreenBuffer;

	m_OffscreenActive = FALSE;

	if (m_FrameBuffer != 0 && m_FrameBuffer->GetDepth() == 16)
	{
		u16 *dst_base = m_Screen->GetDrawBuffer16();
		const unsigned dst_pitch = m_Screen->GetPitchPixels();
		for (unsigned y = 0u; y < h; ++y)
		{
			u16 *dst = dst_base + ((origin_y + y) * dst_pitch) + origin_x;
			const TScreenColor *line = src + (y * stride);
			for (unsigned x = 0u; x < w; ++x)
			{
				dst[x] = (u16) line[x];
			}
		}
		m_Screen->MarkDirtyRect(origin_x, origin_y, w, h);
	}
	else
	{
		for (unsigned y = 0u; y < h; ++y)
		{
			const TScreenColor *line = src + (y * stride);
			for (unsigned x = 0u; x < w; ++x)
			{
				m_Screen->SetPixel(origin_x + x, origin_y + y, line[x]);
			}
		}
	}

	CancelOffscreen();
}

void CGfxTextBoxRenderer::CancelOffscreen(void)
{
	m_OffscreenActive = FALSE;
	m_OffscreenOriginX = 0u;
	m_OffscreenOriginY = 0u;
	m_OffscreenW = 0u;
	m_OffscreenH = 0u;
	m_OffscreenStride = 0u;
	m_OffscreenBuffer = 0;
}

boolean CGfxTextBoxRenderer::IsOffscreenActive(void) const
{
	return m_OffscreenActive;
}

void CGfxTextBoxRenderer::SaveOffscreenState(TGfxOffscreenState *state) const
{
	if (state == 0)
	{
		return;
	}
	state->active = m_OffscreenActive;
	state->origin_x = m_OffscreenOriginX;
	state->origin_y = m_OffscreenOriginY;
	state->w = m_OffscreenW;
	state->h = m_OffscreenH;
	state->stride = m_OffscreenStride;
	state->buffer = m_OffscreenBuffer;
}

void CGfxTextBoxRenderer::RestoreOffscreenState(const TGfxOffscreenState *state)
{
	if (state == 0 || !state->active)
	{
		CancelOffscreen();
		return;
	}
	m_OffscreenActive = TRUE;
	m_OffscreenOriginX = state->origin_x;
	m_OffscreenOriginY = state->origin_y;
	m_OffscreenW = state->w;
	m_OffscreenH = state->h;
	m_OffscreenStride = state->stride;
	m_OffscreenBuffer = state->buffer;
}

void CGfxTextBoxRenderer::FillRect(unsigned x, unsigned y, unsigned w, unsigned h, TScreenColor color)
{
	if (m_Screen == 0 || w == 0u || h == 0u)
	{
		return;
	}

	for (unsigned py = 0; py < h; ++py)
	{
		for (unsigned px = 0; px < w; ++px)
		{
			PutPixel(x + px, y + py, color);
		}
	}
}

void CGfxTextBoxRenderer::DrawRectBorder(unsigned x, unsigned y, unsigned w, unsigned h, TScreenColor color,
					 unsigned thickness)
{
	if (m_Screen == 0 || w == 0u || h == 0u)
	{
		return;
	}

	if (thickness == 0u)
	{
		thickness = 1u;
	}

	if (thickness > w)
	{
		thickness = w;
	}
	if (thickness > h)
	{
		thickness = h;
	}

	FillRect(x, y, w, thickness, color);
	FillRect(x, y + (h - thickness), w, thickness, color);
	FillRect(x, y, thickness, h, color);
	FillRect(x + (w - thickness), y, thickness, h, color);
}

void CGfxTextBoxRenderer::DrawChar(unsigned x, unsigned y, char ch, const TGfxTextStyle &style,
				   unsigned scale_x, unsigned scale_y)
{
	if (m_Screen == 0)
	{
		return;
	}
	if (scale_x == 0u) scale_x = 1u;
	if (scale_y == 0u) scale_y = 1u;
    const unsigned char_w = m_Font.GetCharWidth();
    const unsigned char_h = m_Font.GetCharHeight();

    const unsigned char glyph = (unsigned char) ch;
    if (glyph == kFolderGlyphLeftChar)
    {
        DrawCustomFolderGlyphHalf(this, x, y, style, scale_x, scale_y, char_w, char_h, FALSE);
        return;
    }
    if (glyph == kFolderGlyphRightChar)
    {
        DrawCustomFolderGlyphHalf(this, x, y, style, scale_x, scale_y, char_w, char_h, TRUE);
        return;
    }
    if (glyph == kArrowUpGlyphChar)
    {
        DrawCustomArrowUpGlyph(this, x, y, style, scale_x, scale_y, char_w, char_h);
        return;
    }
    if (glyph == kBlockFullGlyphChar)
    {
        DrawCustomFullBlockGlyph(this, x, y, style, scale_x, scale_y, char_w, char_h);
        return;
    }
    if (glyph == kArrowDownGlyphChar)
    {
        DrawCustomArrowDownGlyph(this, x, y, style, scale_x, scale_y, char_w, char_h);
        return;
    }

	for (unsigned row = 0; row < char_h; ++row)
	{
		const CCharGenerator::TPixelLine line = m_Font.GetPixelLine(ch, row);
		for (unsigned sy = 0; sy < scale_y; ++sy)
		{
			const unsigned dst_y = y + (row * scale_y) + sy;
			for (unsigned col = 0; col < char_w; ++col)
			{
				const boolean on = m_Font.GetPixel(col, line);
				if (!on && !style.opaque_bg)
				{
					continue;
				}
				const TScreenColor color = on ? style.fg : style.bg;
				for (unsigned sx = 0; sx < scale_x; ++sx)
				{
					const unsigned dst_x = x + (col * scale_x) + sx;
					PutPixel(dst_x, dst_y, color);
				}
			}
		}
	}
}

void CGfxTextBoxRenderer::DrawText(unsigned x, unsigned y, const char *text, const TGfxTextStyle &style,
				   unsigned scale_x, unsigned scale_y)
{
	if (m_Screen == 0 || text == 0)
	{
		return;
	}

	if (scale_x == 0u) scale_x = 1u;
	if (scale_y == 0u) scale_y = 1u;

	const unsigned step_x = m_Font.GetCharWidth() * scale_x;
	const unsigned step_y = m_Font.GetCharHeight() * scale_y;
	unsigned cx = x;
	unsigned cy = y;

	for (const char *p = text; *p != '\0';)
	{
		if (*p == '\n')
		{
			cx = x;
			cy += step_y;
            ++p;
			continue;
		}
        unsigned consumed = 0u;
        const unsigned cp = DecodeUtf8Latin1(p, &consumed);
        if (consumed == 0u)
        {
            break;
        }
		DrawChar(cx, cy, (char) (unsigned char) cp, style, scale_x, scale_y);
		cx += step_x;
        p += consumed;
	}
}

void CGfxTextBoxRenderer::DrawMatrix(unsigned x, unsigned y, const TGfxTextMatrix &matrix, const TGfxTextStyle &style,
				     unsigned scale_x, unsigned scale_y)
{
	if (m_Screen == 0 || matrix.cells == 0 || matrix.cols == 0u || matrix.rows == 0u)
	{
		return;
	}

	if (scale_x == 0u) scale_x = 1u;
	if (scale_y == 0u) scale_y = 1u;

	const unsigned step_x = m_Font.GetCharWidth() * scale_x;
	const unsigned step_y = m_Font.GetCharHeight() * scale_y;

	for (unsigned row = 0; row < matrix.rows; ++row)
	{
		for (unsigned col = 0; col < matrix.cols; ++col)
		{
			const char ch = matrix.cells[(row * matrix.cols) + col];
			DrawChar(x + (col * step_x), y + (row * step_y), ch != '\0' ? ch : ' ', style, scale_x, scale_y);
		}
	}
}
