#ifndef SMSBARE_CIRCLE_SMSBARE_GFX_TEXTBOX_H
#define SMSBARE_CIRCLE_SMSBARE_GFX_TEXTBOX_H

#include "combo_screen_device.h"

#include <circle/chargenerator.h>
#include <circle/types.h>

struct TGfxTextStyle
{
	TScreenColor fg;
	TScreenColor bg;
	boolean opaque_bg;
};

struct TGfxTextMatrix
{
	const char *cells;
	unsigned cols;
	unsigned rows;
};

struct TGfxOffscreenState
{
	boolean active;
	unsigned origin_x;
	unsigned origin_y;
	unsigned w;
	unsigned h;
	unsigned stride;
	TScreenColor *buffer;
};

class CGfxTextBoxRenderer
{
public:
	CGfxTextBoxRenderer(CComboScreenDevice *screen = 0);

	void Attach(CComboScreenDevice *screen);
	unsigned GetCharWidth(void) const;
	unsigned GetCharHeight(void) const;

	void FillRect(unsigned x, unsigned y, unsigned w, unsigned h, TScreenColor color);
	void DrawRectBorder(unsigned x, unsigned y, unsigned w, unsigned h, TScreenColor color,
			   unsigned thickness = 1u);
	void DrawChar(unsigned x, unsigned y, char ch, const TGfxTextStyle &style,
		      unsigned scale_x = 1u, unsigned scale_y = 1u);
	void DrawText(unsigned x, unsigned y, const char *text, const TGfxTextStyle &style,
		      unsigned scale_x = 1u, unsigned scale_y = 1u);
	void DrawMatrix(unsigned x, unsigned y, const TGfxTextMatrix &matrix, const TGfxTextStyle &style,
			unsigned scale_x = 1u, unsigned scale_y = 1u);
	void CopyRectFromScreen(unsigned x, unsigned y, unsigned w, unsigned h,
				TScreenColor *buffer, unsigned buffer_stride_pixels);
	void CopyRectFromVisibleScreen(unsigned x, unsigned y, unsigned w, unsigned h,
				      TScreenColor *buffer, unsigned buffer_stride_pixels);
	void BeginOffscreen(unsigned origin_x, unsigned origin_y, unsigned w, unsigned h,
			    TScreenColor *buffer, unsigned buffer_stride_pixels);
	void EndOffscreenBlit(void);
	void CancelOffscreen(void);
	boolean IsOffscreenActive(void) const;
	void SaveOffscreenState(TGfxOffscreenState *state) const;
	void RestoreOffscreenState(const TGfxOffscreenState *state);

private:
	void PutPixel(unsigned x, unsigned y, TScreenColor color);

private:
	CComboScreenDevice *m_Screen;
	CBcmFrameBuffer *m_FrameBuffer;
	CCharGenerator m_Font;
	boolean m_OffscreenActive;
	unsigned m_OffscreenOriginX;
	unsigned m_OffscreenOriginY;
	unsigned m_OffscreenW;
	unsigned m_OffscreenH;
	unsigned m_OffscreenStride;
	TScreenColor *m_OffscreenBuffer;
};

#endif
