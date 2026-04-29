#include "combo_screen_device.h"

#include <circle/devicenameservice.h>
#include <circle/util.h>

static const char DevicePrefix[] = "tty";
static const uintptr kComboScreenDCacheLineSize = 64u;

static void ComboFlushDCacheRange(void *addr, unsigned bytes)
{
	if (addr == 0 || bytes == 0u)
	{
		return;
	}

	uintptr start = (uintptr) addr;
	uintptr end = start + (uintptr) bytes;
	start &= ~(kComboScreenDCacheLineSize - 1u);
	end = (end + (kComboScreenDCacheLineSize - 1u)) & ~(kComboScreenDCacheLineSize - 1u);

	for (uintptr p = start; p < end; p += kComboScreenDCacheLineSize)
	{
		asm volatile ("dc cvac, %0" :: "r" (p) : "memory");
	}

	asm volatile ("dsb ish" ::: "memory");
}

static void ComboFlushDCacheRect565(u16 *base,
					  unsigned pitch_pixels,
					  unsigned x,
					  unsigned y,
					  unsigned width,
					  unsigned height)
{
	if (base == 0 || width == 0u || height == 0u || pitch_pixels == 0u)
	{
		return;
	}

	if (x == 0u && width == pitch_pixels)
	{
		u16 *flush_start = base + (y * pitch_pixels);
		const unsigned flush_pixels = height * pitch_pixels;
		ComboFlushDCacheRange(flush_start, flush_pixels * (unsigned) sizeof(u16));
		return;
	}

	for (unsigned row = 0u; row < height; ++row)
	{
		u16 *flush_start = base + ((y + row) * pitch_pixels) + x;
		ComboFlushDCacheRange(flush_start, width * (unsigned) sizeof(u16));
	}
}

class CComboBackBufferDisplay : public CDisplay
{
public:
	explicit CComboBackBufferDisplay(CComboScreenDevice *pOwner)
	:	CDisplay(RGB565),
		m_pOwner(pOwner)
	{
	}

	unsigned GetWidth(void) const override
	{
		return m_pOwner != 0 ? m_pOwner->GetWidth() : 0u;
	}

	unsigned GetHeight(void) const override
	{
		return m_pOwner != 0 ? m_pOwner->GetHeight() : 0u;
	}

	unsigned GetDepth(void) const override
	{
		return 16u;
	}

	void SetPixel(unsigned nPosX, unsigned nPosY, TRawColor nColor) override
	{
		if (m_pOwner == 0)
		{
			return;
		}
		m_pOwner->WritePixelToDrawPage(nPosX, nPosY, nColor);
	}

	void SetArea(const TArea &rArea, const void *pPixels,
		     TAreaCompletionRoutine *pRoutine = nullptr,
		     void *pParam = nullptr) override
	{
		if (m_pOwner != 0)
		{
			m_pOwner->WriteAreaToDrawPage(rArea, pPixels);
		}
		if (pRoutine != nullptr)
		{
			(*pRoutine)(pParam);
		}
	}

private:
	CComboScreenDevice *m_pOwner;
};

CComboScreenDevice::CComboScreenDevice(unsigned nWidth, unsigned nHeight, const TFont &rFont,
				       CCharGenerator::TFontFlags FontFlags, unsigned nDisplay)
:	m_nInitWidth(nWidth),
	m_nInitHeight(nHeight),
	m_nDisplay(nDisplay),
	m_rFont(rFont),
	m_FontFlags(FontFlags),
	m_pFrameBuffer(0),
	m_pDrawDisplay(0),
	m_pTerminal(0),
	m_bPageFlipEnabled(FALSE),
	m_nFrontPage(0u),
	m_nBackPage(0u),
	m_bBackPageSeeded(TRUE),
	m_bDirty(FALSE),
	m_DirtyX1(0u),
	m_DirtyY1(0u),
	m_DirtyX2(0u),
	m_DirtyY2(0u)
{
}

CComboScreenDevice::~CComboScreenDevice(void)
{
	CDeviceNameService::Get()->RemoveDevice(DevicePrefix, m_nDisplay + 1, FALSE);

	delete m_pTerminal;
	m_pTerminal = 0;

	delete m_pDrawDisplay;
	m_pDrawDisplay = 0;

	delete m_pFrameBuffer;
	m_pFrameBuffer = 0;
}

boolean CComboScreenDevice::Initialize(void)
{
	delete m_pTerminal;
	m_pTerminal = 0;

	delete m_pDrawDisplay;
	m_pDrawDisplay = 0;

	delete m_pFrameBuffer;
	m_pFrameBuffer = 0;

	m_pFrameBuffer = new CBcmFrameBuffer(m_nInitWidth, m_nInitHeight, DEPTH,
						     m_nInitWidth, m_nInitHeight * 2u,
						     m_nDisplay, TRUE);
	if (m_pFrameBuffer == 0)
	{
		return FALSE;
	}

	if (!m_pFrameBuffer->Initialize())
	{
		return FALSE;
	}

	if (m_pFrameBuffer->GetDepth() != DEPTH)
	{
		return FALSE;
	}

	m_pDrawDisplay = new CComboBackBufferDisplay(this);
	if (m_pDrawDisplay == 0)
	{
		return FALSE;
	}

	m_pTerminal = new CTerminalDevice(m_pDrawDisplay, m_nDisplay + 100, m_rFont, m_FontFlags);
	if (m_pTerminal == 0)
	{
		return FALSE;
	}

	ResetPresentationState();
	ClearAllPages((TScreenColor) BLACK_COLOR, TRUE);
	if (!m_pTerminal->Initialize())
	{
		return FALSE;
	}
	ClearDirty();

	if (!CDeviceNameService::Get()->GetDevice(DevicePrefix, m_nDisplay + 1, FALSE))
	{
		CDeviceNameService::Get()->AddDevice(DevicePrefix, m_nDisplay + 1, this, FALSE);
	}

	return TRUE;
}

boolean CComboScreenDevice::Resize(unsigned nWidth, unsigned nHeight)
{
	m_nInitWidth = nWidth;
	m_nInitHeight = nHeight;
	return Initialize();
}

unsigned CComboScreenDevice::GetWidth(void) const
{
	if (m_pFrameBuffer != 0 && m_pFrameBuffer->GetWidth() > 0u)
	{
		return m_pFrameBuffer->GetWidth();
	}
	if (m_pTerminal != 0 && m_pTerminal->GetWidth() > 0u)
	{
		return m_pTerminal->GetWidth();
	}
	return m_nInitWidth;
}

unsigned CComboScreenDevice::GetHeight(void) const
{
	if (m_pFrameBuffer != 0 && m_pFrameBuffer->GetHeight() > 0u)
	{
		return m_pFrameBuffer->GetHeight();
	}
	if (m_pTerminal != 0 && m_pTerminal->GetHeight() > 0u)
	{
		return m_pTerminal->GetHeight();
	}
	return m_nInitHeight;
}

unsigned CComboScreenDevice::GetColumns(void) const
{
	return (m_pTerminal != 0 && m_pTerminal->GetColumns() > 0u) ? m_pTerminal->GetColumns() : 0u;
}

unsigned CComboScreenDevice::GetRows(void) const
{
	return (m_pTerminal != 0 && m_pTerminal->GetRows() > 0u) ? m_pTerminal->GetRows() : 0u;
}

CBcmFrameBuffer *CComboScreenDevice::GetFrameBuffer(void)
{
	return m_pFrameBuffer;
}

int CComboScreenDevice::Write(const void *pBuffer, size_t nCount)
{
	return m_pTerminal != 0 ? m_pTerminal->Write(pBuffer, nCount) : 0;
}

void CComboScreenDevice::SetPixel(unsigned nPosX, unsigned nPosY, TScreenColor Color)
{
	if (m_pTerminal != 0)
	{
		m_pTerminal->SetPixel(nPosX, nPosY, Color);
	}
}

TScreenColor CComboScreenDevice::GetPixel(unsigned nPosX, unsigned nPosY)
{
	return m_pTerminal != 0 ? m_pTerminal->GetRawPixel(nPosX, nPosY) : (TScreenColor) 0u;
}

void CComboScreenDevice::Rotor(unsigned nIndex, unsigned nCount)
{
	static u8 Pos[4][2] =
	{
		{0, 1},
		{1, 0},
		{2, 1},
		{1, 2}
	};

	nIndex %= 4u;
	const unsigned nPosX = 2u + GetWidth() - (nIndex + 1u) * 8u;
	const unsigned nPosY = 2u;

	nCount &= 3u;
	SetPixel(nPosX + Pos[nCount][0], nPosY + Pos[nCount][1], (TScreenColor) BLACK_COLOR);

	nCount++;
	nCount &= 3u;
	SetPixel(nPosX + Pos[nCount][0], nPosY + Pos[nCount][1], (TScreenColor) HIGH_COLOR);
}

void CComboScreenDevice::SetCursorBlock(boolean bCursorBlock)
{
	if (m_pTerminal != 0)
	{
		m_pTerminal->SetCursorBlock(bCursorBlock);
	}
}

void CComboScreenDevice::Update(unsigned nMillis)
{
	if (m_pTerminal != 0)
	{
		m_pTerminal->Update(nMillis);
	}
}

u16 *CComboScreenDevice::GetDrawBuffer16(void)
{
	return (u16 *) GetDrawPageBase();
}

const u16 *CComboScreenDevice::GetVisibleBuffer16(void) const
{
	return (const u16 *) GetVisiblePageBase();
}

unsigned CComboScreenDevice::GetPitchPixels(void) const
{
	return m_pFrameBuffer != 0 ? (m_pFrameBuffer->GetPitch() / sizeof(u16)) : 0u;
}

unsigned CComboScreenDevice::GetVisiblePageIndex(void) const
{
	return m_bPageFlipEnabled ? m_nFrontPage : 0u;
}

unsigned CComboScreenDevice::GetDrawPageIndex(void) const
{
	return m_bPageFlipEnabled ? m_nBackPage : 0u;
}

void CComboScreenDevice::MarkDirtyRect(unsigned x, unsigned y, unsigned w, unsigned h)
{
	if (w == 0u || h == 0u || m_pFrameBuffer == 0)
	{
		return;
	}

	const unsigned screen_w = GetWidth();
	const unsigned screen_h = GetHeight();
	if (x >= screen_w || y >= screen_h)
	{
		return;
	}

	unsigned x2 = x + w;
	unsigned y2 = y + h;
	if (x2 > screen_w) x2 = screen_w;
	if (y2 > screen_h) y2 = screen_h;
	if (x2 <= x || y2 <= y)
	{
		return;
	}

	if (!m_bDirty)
	{
		m_DirtyX1 = x;
		m_DirtyY1 = y;
		m_DirtyX2 = x2;
		m_DirtyY2 = y2;
		m_bDirty = TRUE;
		return;
	}

	if (x < m_DirtyX1) m_DirtyX1 = x;
	if (y < m_DirtyY1) m_DirtyY1 = y;
	if (x2 > m_DirtyX2) m_DirtyX2 = x2;
	if (y2 > m_DirtyY2) m_DirtyY2 = y2;
}

void CComboScreenDevice::ClearDirty(void)
{
	m_bDirty = FALSE;
	m_DirtyX1 = 0u;
	m_DirtyY1 = 0u;
	m_DirtyX2 = 0u;
	m_DirtyY2 = 0u;
}

boolean CComboScreenDevice::IsDirty(void) const
{
	return m_bDirty;
}

boolean CComboScreenDevice::PresentIfDirty(boolean dcache_enabled, boolean wait_vsync)
{
	if (!m_bDirty || m_pFrameBuffer == 0)
	{
		return FALSE;
	}

	if (!m_bPageFlipEnabled)
	{
		if (dcache_enabled)
		{
			u16 *draw_base = GetDrawBuffer16();
			const unsigned pitch_pixels = GetPitchPixels();
			const unsigned flush_w = m_DirtyX2 - m_DirtyX1;
			const unsigned flush_h = m_DirtyY2 - m_DirtyY1;
			ComboFlushDCacheRect565(draw_base, pitch_pixels,
				m_DirtyX1, m_DirtyY1, flush_w, flush_h);
		}

		ClearDirty();
		return TRUE;
	}

	if (dcache_enabled)
	{
		u16 *draw_base = GetDrawBuffer16();
		const unsigned pitch_pixels = GetPitchPixels();
		u16 *flush_start = draw_base + (m_DirtyY1 * pitch_pixels) + m_DirtyX1;
		const unsigned flush_w = m_DirtyX2 - m_DirtyX1;
		const unsigned flush_h = m_DirtyY2 - m_DirtyY1;
		const unsigned flush_pixels = ((flush_h - 1u) * pitch_pixels) + flush_w;
		ComboFlushDCacheRange(flush_start, flush_pixels * (unsigned) sizeof(u16));
	}

	const unsigned dirty_x1 = m_DirtyX1;
	const unsigned dirty_y1 = m_DirtyY1;
	const unsigned dirty_x2 = m_DirtyX2;
	const unsigned dirty_y2 = m_DirtyY2;

	if (wait_vsync)
	{
		(void) m_pFrameBuffer->WaitForVerticalSync();
	}

	const unsigned page_height = m_pFrameBuffer->GetHeight();
	const boolean offset_ok = m_pFrameBuffer->SetVirtualOffset(0u, m_nBackPage * page_height);

	m_nFrontPage = m_nBackPage;
	m_nBackPage ^= 1u;

	u16 *draw_base = GetDrawBuffer16();
	const u16 *visible_base = GetVisibleBuffer16();
	if (draw_base != 0 && visible_base != 0)
	{
		const unsigned pitch_pixels = GetPitchPixels();
		const unsigned copy_w = dirty_x2 > dirty_x1 ? (dirty_x2 - dirty_x1) : 0u;
		const unsigned copy_h = dirty_y2 > dirty_y1 ? (dirty_y2 - dirty_y1) : 0u;
		const unsigned dirty_area = copy_w * copy_h;
		const unsigned page_area = page_height * pitch_pixels;
		const boolean skip_copy = dirty_area * 4u >= page_area;
		if (!skip_copy)
		{
			for (unsigned row = 0u; row < copy_h; ++row)
			{
				u16 *dst = draw_base + ((dirty_y1 + row) * pitch_pixels) + dirty_x1;
				const u16 *src = visible_base + ((dirty_y1 + row) * pitch_pixels) + dirty_x1;
				memcpy(dst, src, copy_w * sizeof(u16));
			}
			m_bBackPageSeeded = TRUE;
		}
		else
		{
			m_bBackPageSeeded = FALSE;
		}
	}
	(void) offset_ok;
	ClearDirty();
	return TRUE;
}

void CComboScreenDevice::ClearAllPages(TScreenColor color, boolean dcache_enabled)
{
	if (m_pFrameBuffer == 0 || m_pFrameBuffer->GetDepth() != 16)
	{
		return;
	}

	u16 *base = (u16 *) (uintptr) m_pFrameBuffer->GetBuffer();
	const unsigned pitch_pixels = GetPitchPixels();
	const unsigned page_pixels = m_pFrameBuffer->GetHeight() * pitch_pixels;
	for (unsigned i = 0u; i < page_pixels * 2u; ++i)
	{
		base[i] = (u16) color;
	}
	if (dcache_enabled)
	{
		ComboFlushDCacheRange(base, page_pixels * 2u * (unsigned) sizeof(u16));
	}
	m_bBackPageSeeded = TRUE;
}

CDisplay::TRawColor *CComboScreenDevice::GetPageBase(void) const
{
	return m_pFrameBuffer != 0 ? (CDisplay::TRawColor *) (uintptr) m_pFrameBuffer->GetBuffer() : 0;
}

CDisplay::TRawColor *CComboScreenDevice::GetDrawPageBase(void) const
{
	if (m_pFrameBuffer == 0)
	{
		return 0;
	}
	const uintptr base = (uintptr) m_pFrameBuffer->GetBuffer();
	const unsigned page_index = m_bPageFlipEnabled ? m_nBackPage : 0u;
	const uintptr offset = (uintptr) page_index * (uintptr) m_pFrameBuffer->GetHeight() * (uintptr) m_pFrameBuffer->GetPitch();
	return (CDisplay::TRawColor *) (base + offset);
}

CDisplay::TRawColor *CComboScreenDevice::GetVisiblePageBase(void) const
{
	if (m_pFrameBuffer == 0)
	{
		return 0;
	}
	const uintptr base = (uintptr) m_pFrameBuffer->GetBuffer();
	const unsigned page_index = m_bPageFlipEnabled ? m_nFrontPage : 0u;
	const uintptr offset = (uintptr) page_index * (uintptr) m_pFrameBuffer->GetHeight() * (uintptr) m_pFrameBuffer->GetPitch();
	return (CDisplay::TRawColor *) (base + offset);
}

void CComboScreenDevice::WriteAreaToDrawPage(const CDisplay::TArea &rArea, const void *pPixels)
{
	if (m_pFrameBuffer == 0 || pPixels == 0 || m_pFrameBuffer->GetDepth() != 16)
	{
		return;
	}

	EnsureBackPageSeeded();

	const unsigned width = rArea.x2 - rArea.x1 + 1u;
	const unsigned height = rArea.y2 - rArea.y1 + 1u;
	u16 *dst_base = GetDrawBuffer16();
	const unsigned pitch_pixels = GetPitchPixels();
	const u16 *src = (const u16 *) pPixels;
	for (unsigned row = 0u; row < height; ++row)
	{
		u16 *dst = dst_base + ((rArea.y1 + row) * pitch_pixels) + rArea.x1;
		const u16 *line = src + (row * width);
		memcpy(dst, line, width * sizeof(u16));
	}
	MarkDirtyRect(rArea.x1, rArea.y1, width, height);
}

void CComboScreenDevice::WritePixelToDrawPage(unsigned nPosX, unsigned nPosY, CDisplay::TRawColor nColor)
{
	if (m_pFrameBuffer == 0 || m_pFrameBuffer->GetDepth() != 16)
	{
		return;
	}

	EnsureBackPageSeeded();

	u16 *dst = GetDrawBuffer16();
	const unsigned pitch_pixels = GetPitchPixels();
	dst[(nPosY * pitch_pixels) + nPosX] = (u16) nColor;
	MarkDirtyRect(nPosX, nPosY, 1u, 1u);
}

void CComboScreenDevice::EnsureBackPageSeeded(void)
{
	if (!m_bPageFlipEnabled || m_bBackPageSeeded || m_pFrameBuffer == 0 || m_pFrameBuffer->GetDepth() != 16)
	{
		return;
	}

	u16 *draw_base = GetDrawBuffer16();
	const u16 *visible_base = GetVisibleBuffer16();
	if (draw_base == 0 || visible_base == 0)
	{
		return;
	}

	const unsigned page_pixels = m_pFrameBuffer->GetHeight() * GetPitchPixels();
	memcpy(draw_base, visible_base, page_pixels * sizeof(u16));
	m_bBackPageSeeded = TRUE;
}

void CComboScreenDevice::ResetPresentationState(void)
{
	m_nFrontPage = 0u;
	m_nBackPage = m_bPageFlipEnabled ? 1u : 0u;
	m_bBackPageSeeded = TRUE;
	if (m_pFrameBuffer != 0)
	{
		(void) m_pFrameBuffer->SetVirtualOffset(0u, 0u);
	}
	ClearDirty();
}
