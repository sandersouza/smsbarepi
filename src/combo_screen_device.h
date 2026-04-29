#ifndef SMSBARE_CIRCLE_SMSBARE_COMBO_SCREEN_DEVICE_H
#define SMSBARE_CIRCLE_SMSBARE_COMBO_SCREEN_DEVICE_H

#include <circle/bcmframebuffer.h>
#include <circle/chargenerator.h>
#include <circle/device.h>
#include <circle/screen.h>
#include <circle/terminal.h>
#include <circle/types.h>

class CComboBackBufferDisplay;

class CComboScreenDevice : public CDevice
{
public:
	CComboScreenDevice(unsigned nWidth, unsigned nHeight, const TFont &rFont = DEFAULT_FONT,
			  CCharGenerator::TFontFlags FontFlags = CCharGenerator::FontFlagsNone,
			  unsigned nDisplay = 0);
	~CComboScreenDevice(void);

	boolean Initialize(void);
	boolean Resize(unsigned nWidth, unsigned nHeight);

	unsigned GetWidth(void) const;
	unsigned GetHeight(void) const;
	unsigned GetColumns(void) const;
	unsigned GetRows(void) const;

	CBcmFrameBuffer *GetFrameBuffer(void);
	int Write(const void *pBuffer, size_t nCount);
	void SetPixel(unsigned nPosX, unsigned nPosY, TScreenColor Color);
	TScreenColor GetPixel(unsigned nPosX, unsigned nPosY);
	void Rotor(unsigned nIndex, unsigned nCount);
	void SetCursorBlock(boolean bCursorBlock);
	void Update(unsigned nMillis = 0);

	u16 *GetDrawBuffer16(void);
	const u16 *GetVisibleBuffer16(void) const;
	unsigned GetPitchPixels(void) const;
	unsigned GetVisiblePageIndex(void) const;
	unsigned GetDrawPageIndex(void) const;

	void MarkDirtyRect(unsigned x, unsigned y, unsigned w, unsigned h);
	void ClearDirty(void);
	boolean IsDirty(void) const;
	boolean PresentIfDirty(boolean dcache_enabled, boolean wait_vsync = TRUE);
	void ClearAllPages(TScreenColor color, boolean dcache_enabled);

private:
	friend class CComboBackBufferDisplay;

	CDisplay::TRawColor *GetPageBase(void) const;
	CDisplay::TRawColor *GetDrawPageBase(void) const;
	CDisplay::TRawColor *GetVisiblePageBase(void) const;
	void WriteAreaToDrawPage(const CDisplay::TArea &rArea, const void *pPixels);
	void WritePixelToDrawPage(unsigned nPosX, unsigned nPosY, CDisplay::TRawColor nColor);
	void EnsureBackPageSeeded(void);
	void ResetPresentationState(void);

private:
	unsigned m_nInitWidth;
	unsigned m_nInitHeight;
	unsigned m_nDisplay;
	const TFont &m_rFont;
	CCharGenerator::TFontFlags m_FontFlags;
	CBcmFrameBuffer *m_pFrameBuffer;
	CComboBackBufferDisplay *m_pDrawDisplay;
	CTerminalDevice *m_pTerminal;
	boolean m_bPageFlipEnabled;
	unsigned m_nFrontPage;
	unsigned m_nBackPage;
	boolean m_bBackPageSeeded;
	boolean m_bDirty;
	unsigned m_DirtyX1;
	unsigned m_DirtyY1;
	unsigned m_DirtyX2;
	unsigned m_DirtyY2;
};

#endif
