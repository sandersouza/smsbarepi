#include "combo_menu.h"
#include "combo_locale.h"
#include "viewport_screenshot.h"
#include <string.h>

static constexpr unsigned kPauseMenuBackgroundRgb = 0x0057FFu;
static constexpr unsigned kPauseMenuForegroundRgb = 0xffffffu;

static constexpr unsigned ComboRgbRed(unsigned rgb)
{
    return (rgb >> 16) & 0xFFu;
}

static constexpr unsigned ComboRgbGreen(unsigned rgb)
{
    return (rgb >> 8) & 0xFFu;
}

static constexpr unsigned ComboRgbBlue(unsigned rgb)
{
    return rgb & 0xFFu;
}

#if DEPTH == 32
static constexpr TScreenColor ComboMenuColorFromRgb(unsigned rgb)
{
    return COLOR32(ComboRgbRed(rgb), ComboRgbGreen(rgb), ComboRgbBlue(rgb), 0xFFu);
}
#else
static constexpr TScreenColor ComboMenuColorFromRgb(unsigned rgb)
{
    return COLOR16(ComboRgbRed(rgb) >> 3, ComboRgbGreen(rgb) >> 3, ComboRgbBlue(rgb) >> 3);
}
#endif

static constexpr TScreenColor kPauseMenuBgColor = ComboMenuColorFromRgb(kPauseMenuBackgroundRgb);
static constexpr TScreenColor kPauseMenuFgColor = ComboMenuColorFromRgb(kPauseMenuForegroundRgb);

static const unsigned kMenuRepeatDelayFrames = 12u;  /* ~200ms em 60Hz */
static const unsigned kMenuRepeatPeriodFrames = 2u;  /* ~30 passos/s em 60Hz */

static boolean ComboConsumeHoldRepeat(boolean pressed, boolean last_pressed, unsigned *hold_frames)
{
    if (hold_frames == 0)
    {
        return (pressed && !last_pressed) ? TRUE : FALSE;
    }

    if (pressed)
    {
        if (!last_pressed)
        {
            *hold_frames = 0u;
            return TRUE;
        }

        ++*hold_frames;
        if (*hold_frames >= kMenuRepeatDelayFrames)
        {
            const unsigned tick = *hold_frames - kMenuRepeatDelayFrames;
            if ((tick % kMenuRepeatPeriodFrames) == 0u)
            {
                return TRUE;
            }
        }
    }
    else
    {
        *hold_frames = 0u;
    }

    return FALSE;
}

static unsigned ComboStrLen(const char *text)
{
    if (text == 0)
    {
        return 0u;
    }

    unsigned len = 0u;
    while (text[len] != '\0')
    {
        ++len;
    }

    return len;
}

static void ComboSliceCopy(char *dst, unsigned dst_size, const char *src, unsigned start, unsigned len)
{
    if (dst == 0 || dst_size == 0u)
    {
        return;
    }

    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    unsigned i = 0u;
    while (i + 1u < dst_size && i < len && src[start + i] != '\0')
    {
        dst[i] = src[start + i];
        ++i;
    }
    dst[i] = '\0';
}

static unsigned ComboFindFlagStart(const char *label)
{
    const unsigned len = ComboStrLen(label);
    if (len < 4u)
    {
        return len;
    }

    if (label[len - 1u] != ']')
    {
        return len;
    }

    for (unsigned i = len - 1u; i > 1u; --i)
    {
        if (label[i] == '[' && label[i - 1u] == ' ')
        {
            return i - 1u;
        }
    }

    return len;
}

static int ComboTextEquals(const char *a, const char *b)
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

static unsigned ComboFindTabSplit(const char *label)
{
    const unsigned len = ComboStrLen(label);
    for (unsigned i = 0u; i < len; ++i)
    {
        if (label[i] == '\t')
        {
            return i;
        }
    }
    return len;
}

static unsigned AudioGainUiLevelFromPercent(unsigned percent)
{
    if (percent > 100u)
    {
        percent = 100u;
    }
    return (percent * 9u + 50u) / 100u;
}

static unsigned AudioGainPercentFromUiLevel(unsigned level)
{
    if (level > 9u)
    {
        level = 9u;
    }
    return (level * 100u + 4u) / 9u;
}

static void ComboClearPixels(TScreenColor *buffer, unsigned count)
{
    if (buffer == 0 || count == 0u)
    {
        return;
    }
    for (unsigned i = 0u; i < count; ++i)
    {
        buffer[i] = (TScreenColor) 0u;
    }
}

void CComboPauseMenu::CopySmallText(char *dst, unsigned dst_size, const char *src)
{
    if (dst == 0 || dst_size == 0u)
    {
        return;
    }
    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    unsigned i = 0u;
    while (src[i] != '\0' && i + 1u < dst_size)
    {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

CComboPauseMenu::CComboPauseMenu(void)
: m_Row(1),
  m_Col(1),
  m_Selected(0),
  m_LastDrawnSelected(0xFFFFFFFFu),
    m_Visible(FALSE),
    m_View(MenuViewRoot),
    m_ScalePercent(300u),
    m_ScaleMaxFitEnabled(FALSE),
    m_MaxScalePercent(300u),
    m_Language(ComboLanguageEN),
    m_ScanlineMode(0u),
    m_ColorArtifactsEnabled(TRUE),
    m_Gfx9000Enabled(FALSE),
    m_DiskRomEnabled(TRUE),
    m_CoreName{'s', 'm', 's', '\0'},
    m_BootMode(0u),
    m_MachineProfile(0u),
    m_ProcessorMode(0u),
    m_RamMapperKb(512u),
    m_MegaRamKb(0u),
    m_JoystickMapCodes{0u},
    m_JoystickMapEditCodes{0u},
    m_JoystickMapDirty(FALSE),
    m_LastJoystickCaptureCode(0u),
    m_JoystickMapReturnSelected(0u),
    m_FmMusicEnabled(TRUE),
    m_SccCartEnabled(TRUE),
    m_SccDualCartEnabled(FALSE),
    m_SccDualCartAvailable(TRUE),
    m_AudioGainPercent(50u),
    m_LoadRomSlot1Name{0},
    m_LoadRomSlot2Name{0},
    m_LoadDiskDrive1Name{0},
    m_LoadDiskDrive2Name{0},
    m_LoadCassetteName{0},
    m_FileBrowser(),
    m_RomBrowserItems{{0}},
    m_RomBrowserItemLabels{{0}},
    m_RomBrowserEmptyLabel{0},
    m_HasPendingRomSelection(FALSE),
    m_PendingRomSlot(0u),
    m_PendingRomPath{0},
    m_HasPendingRomBrowserOpen(FALSE),
    m_PendingRomBrowserSlot(0u),
    m_PendingRomBrowserPath{0},
    m_HasPendingDiskSelection(FALSE),
    m_PendingDiskDrive(0u),
    m_PendingDiskPath{0},
    m_HasPendingDiskBrowserOpen(FALSE),
    m_PendingDiskBrowserDrive(0u),
    m_PendingDiskBrowserPath{0},
    m_HasPendingCassetteSelection(FALSE),
    m_PendingCassettePath{0},
    m_HasPendingCassetteBrowserOpen(FALSE),
    m_PendingCassetteBrowserPath{0},
    m_LastBackPressed(FALSE),
    m_LastDrawnView(MenuViewRoot),
    m_LastDrawnScalePercent(0u),
    m_LastDrawnMaxScalePercent(0u),
    m_LastDrawnLanguage(ComboLanguageEN),
    m_LastDrawnScanlineMode(0u),
    m_LastDrawnColorArtifactsEnabled(TRUE),
    m_LastDrawnGfx9000Enabled(FALSE),
    m_LastDrawnDiskRomEnabled(TRUE),
    m_LastDrawnCoreName{0},
    m_LastDrawnBootMode(0u),
    m_LastDrawnMachineProfile(0u),
    m_LastDrawnProcessorMode(0u),
    m_LastDrawnRamMapperKb(0u),
    m_LastDrawnMegaRamKb(0u),
    m_LastDrawnFmMusic(TRUE),
    m_LastDrawnSccCart(TRUE),
    m_LastDrawnSccDualCart(FALSE),
    m_LastDrawnSccDualCartAvailable(TRUE),
    m_LastDrawnAudioGainPercent(50u),
    m_LastDrawnLoadRomSlot1Name{0},
    m_LastDrawnLoadRomSlot2Name{0},
    m_LastDrawnLoadDiskDrive1Name{0},
    m_LastDrawnLoadDiskDrive2Name{0},
    m_LastDrawnLoadCassetteName{0},
    m_LastDrawnBrowserTop(0xFFFFFFFFu),
    m_LastDrawnBrowserCount(0xFFFFFFFFu),
    m_LastDrawnBrowserSelectedAbs(0xFFFFFFFFu),
    m_LastDrawnBrowserMode(0xFFFFFFFFu),
    m_LastDrawnBrowserSlot(0xFFFFFFFFu),
    m_LastDrawnBrowserContentStamp(0xFFFFFFFFu),
    m_BrowserWarmupRedraws(0u),
    m_ForceRedraw(TRUE),
    m_ForceHighlightRedraw(FALSE),
    m_ForceContentRedraw(FALSE),
    m_SuppressNextRender(FALSE),
    m_RequestBackgroundRestore(FALSE),
    m_LastBoxX(0),
    m_LastBoxY(0),
    m_LastBoxW(0),
    m_LastBoxH(0),
    m_LastBoxValid(FALSE),
    m_UpHoldFrames(0u),
    m_DownHoldFrames(0u),
    m_LeftHoldFrames(0u),
    m_RightHoldFrames(0u),
    m_LastUpPressed(FALSE),
    m_LastDownPressed(FALSE),
    m_LastPageUpPressed(FALSE),
    m_LastPageDownPressed(FALSE),
    m_LastLeftPressed(FALSE),
    m_LastRightPressed(FALSE),
    m_LastEnterPressed(FALSE),
    m_LastDeletePressed(FALSE),
    m_MenuBackbuffer(0),
    m_MenuBackbufferCapacityPixels(0u),
    m_MenuBackbufferW(0u),
    m_MenuBackbufferH(0u),
    m_MenuBackbufferX(0u),
    m_MenuBackbufferY(0u)
{
    CopySmallText(m_LoadRomSlot1Name, sizeof(m_LoadRomSlot1Name), "");
    CopySmallText(m_LoadRomSlot2Name, sizeof(m_LoadRomSlot2Name), "");
    CopySmallText(m_LoadDiskDrive1Name, sizeof(m_LoadDiskDrive1Name), "");
    CopySmallText(m_LoadDiskDrive2Name, sizeof(m_LoadDiskDrive2Name), "");
    CopySmallText(m_LoadCassetteName, sizeof(m_LoadCassetteName), "");
    CopySmallText(m_RomBrowserEmptyLabel, sizeof(m_RomBrowserEmptyLabel), "");
    m_HasPendingRomSelection = FALSE;
    m_PendingRomSlot = 0u;
    CopySmallText(m_PendingRomPath, sizeof(m_PendingRomPath), "");
    m_HasPendingRomBrowserOpen = FALSE;
    m_PendingRomBrowserSlot = 0u;
    CopySmallText(m_PendingRomBrowserPath, sizeof(m_PendingRomBrowserPath), "");
    m_HasPendingDiskSelection = FALSE;
    m_PendingDiskDrive = 0u;
    CopySmallText(m_PendingDiskPath, sizeof(m_PendingDiskPath), "");
    m_HasPendingDiskBrowserOpen = FALSE;
    m_PendingDiskBrowserDrive = 0u;
    CopySmallText(m_PendingDiskBrowserPath, sizeof(m_PendingDiskBrowserPath), "");
    m_HasPendingCassetteSelection = FALSE;
    CopySmallText(m_PendingCassettePath, sizeof(m_PendingCassettePath), "");
    m_HasPendingCassetteBrowserOpen = FALSE;
    CopySmallText(m_PendingCassetteBrowserPath, sizeof(m_PendingCassetteBrowserPath), "");
    CopySmallText(m_LastDrawnLoadRomSlot1Name, sizeof(m_LastDrawnLoadRomSlot1Name), "");
    CopySmallText(m_LastDrawnLoadRomSlot2Name, sizeof(m_LastDrawnLoadRomSlot2Name), "");
    CopySmallText(m_LastDrawnLoadDiskDrive1Name, sizeof(m_LastDrawnLoadDiskDrive1Name), "");
    CopySmallText(m_LastDrawnLoadDiskDrive2Name, sizeof(m_LastDrawnLoadDiskDrive2Name), "");
    CopySmallText(m_LastDrawnLoadCassetteName, sizeof(m_LastDrawnLoadCassetteName), "");
}

CComboPauseMenu::~CComboPauseMenu(void)
{
    if (m_MenuBackbuffer != 0)
    {
        delete[] m_MenuBackbuffer;
        m_MenuBackbuffer = 0;
        m_MenuBackbufferCapacityPixels = 0u;
    }
    m_MenuBackbufferW = 0u;
    m_MenuBackbufferH = 0u;
    m_MenuBackbufferX = 0u;
    m_MenuBackbufferY = 0u;
}

void CComboPauseMenu::ResetRepeatState(void)
{
    m_UpHoldFrames = 0u;
    m_DownHoldFrames = 0u;
    m_LeftHoldFrames = 0u;
    m_RightHoldFrames = 0u;
}

void CComboPauseMenu::Reset(void)
{
    m_Row = 1;
    m_Col = 1;
    m_View = MenuViewRoot;
    m_Selected = FindFirstSelectable();
    m_LastDrawnSelected = 0xFFFFFFFFu;
    m_Visible = FALSE;
    m_LastBackPressed = FALSE;
    m_LastDrawnView = MenuViewRoot;
    m_LastDrawnScalePercent = m_ScalePercent;
    m_LastDrawnScaleMaxFitEnabled = m_ScaleMaxFitEnabled;
    m_LastDrawnMaxScalePercent = m_MaxScalePercent;
    m_LastDrawnLanguage = m_Language;
    m_LastDrawnScanlineMode = m_ScanlineMode;
    m_LastDrawnColorArtifactsEnabled = m_ColorArtifactsEnabled;
    m_LastDrawnGfx9000Enabled = m_Gfx9000Enabled;
    m_LastDrawnDiskRomEnabled = m_DiskRomEnabled;
    CopySmallText(m_LastDrawnCoreName, sizeof(m_LastDrawnCoreName), m_CoreName);
    m_LastDrawnBootMode = m_BootMode;
    m_LastDrawnMachineProfile = m_MachineProfile;
    m_LastDrawnProcessorMode = m_ProcessorMode;
    m_LastDrawnFmMusic = m_FmMusicEnabled;
    m_LastDrawnSccCart = m_SccCartEnabled;
    m_LastDrawnSccDualCart = m_SccDualCartEnabled;
    m_LastDrawnSccDualCartAvailable = m_SccDualCartAvailable;
    m_LastDrawnAudioGainPercent = m_AudioGainPercent;
    m_LastDrawnBrowserTop = 0xFFFFFFFFu;
    m_LastDrawnBrowserCount = 0xFFFFFFFFu;
    m_LastDrawnBrowserSelectedAbs = 0xFFFFFFFFu;
    m_LastDrawnBrowserMode = 0xFFFFFFFFu;
    m_LastDrawnBrowserSlot = 0xFFFFFFFFu;
    m_LastDrawnBrowserContentStamp = 0xFFFFFFFFu;
    m_BrowserWarmupRedraws = 0u;
    m_FileBrowser.Reset();
    m_HasPendingRomSelection = FALSE;
    m_PendingRomSlot = 0u;
    CopySmallText(m_PendingRomPath, sizeof(m_PendingRomPath), "");
    m_HasPendingRomBrowserOpen = FALSE;
    m_PendingRomBrowserSlot = 0u;
    CopySmallText(m_PendingRomBrowserPath, sizeof(m_PendingRomBrowserPath), "");
    m_HasPendingDiskSelection = FALSE;
    m_PendingDiskDrive = 0u;
    CopySmallText(m_PendingDiskPath, sizeof(m_PendingDiskPath), "");
    m_HasPendingDiskBrowserOpen = FALSE;
    m_PendingDiskBrowserDrive = 0u;
    CopySmallText(m_PendingDiskBrowserPath, sizeof(m_PendingDiskBrowserPath), "");
    CopySmallText(m_LastDrawnLoadRomSlot1Name, sizeof(m_LastDrawnLoadRomSlot1Name), m_LoadRomSlot1Name);
    CopySmallText(m_LastDrawnLoadRomSlot2Name, sizeof(m_LastDrawnLoadRomSlot2Name), m_LoadRomSlot2Name);
    CopySmallText(m_LastDrawnLoadDiskDrive1Name, sizeof(m_LastDrawnLoadDiskDrive1Name), m_LoadDiskDrive1Name);
    CopySmallText(m_LastDrawnLoadDiskDrive2Name, sizeof(m_LastDrawnLoadDiskDrive2Name), m_LoadDiskDrive2Name);
    CopySmallText(m_LastDrawnLoadCassetteName, sizeof(m_LastDrawnLoadCassetteName), m_LoadCassetteName);
    m_ForceRedraw = TRUE;
    m_ForceHighlightRedraw = FALSE;
    m_ForceContentRedraw = FALSE;
    m_SuppressNextRender = FALSE;
    m_RequestBackgroundRestore = FALSE;
    m_JoystickMapDirty = FALSE;
    m_LastJoystickCaptureCode = 0u;
    m_JoystickMapReturnSelected = 0u;
    for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
    {
        m_JoystickMapEditCodes[i] = m_JoystickMapCodes[i];
    }
    m_LastBoxValid = FALSE;
    ResetRepeatState();
    m_LastUpPressed = FALSE;
    m_LastDownPressed = FALSE;
    m_LastPageUpPressed = FALSE;
    m_LastPageDownPressed = FALSE;
    m_LastLeftPressed = FALSE;
    m_LastRightPressed = FALSE;
    m_LastEnterPressed = FALSE;
    m_LastDeletePressed = FALSE;
    m_MenuBackbufferW = 0u;
    m_MenuBackbufferH = 0u;
    m_MenuBackbufferX = 0u;
    m_MenuBackbufferY = 0u;
}

void CComboPauseMenu::SetScalePercent(unsigned percent)
{
    if (percent <= 200u)
    {
        percent = 200u;
    }
    else if (percent <= 250u)
    {
        percent = 250u;
    }
    else
    {
        percent = 300u;
    }
    if (percent > m_MaxScalePercent) percent = m_MaxScalePercent;

    if (m_ScalePercent != percent)
    {
        m_ScalePercent = percent;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetScaleMaxFitEnabled(boolean enabled)
{
    (void) enabled;
    if (m_ScaleMaxFitEnabled != FALSE)
    {
        m_ScaleMaxFitEnabled = FALSE;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetMaxScalePercent(unsigned percent)
{
    if (percent <= 200u)
    {
        percent = 200u;
    }
    else if (percent <= 250u)
    {
        percent = 250u;
    }
    else
    {
        percent = 300u;
    }
    if (m_MaxScalePercent != percent)
    {
        m_MaxScalePercent = percent;
        if (m_ScalePercent > m_MaxScalePercent)
        {
            m_ScalePercent = m_MaxScalePercent;
        }
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetLanguage(unsigned language)
{
    language = combo_locale_clamp_language(language);
    if (m_Language != language)
    {
        m_Language = language;
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetLanguage(void) const
{
    return m_Language;
}

unsigned CComboPauseMenu::GetScalePercent(void) const
{
    return m_ScalePercent;
}

boolean CComboPauseMenu::IsScaleMaxFitEnabled(void) const
{
    return FALSE;
}

void CComboPauseMenu::SetScanlineMode(unsigned mode)
{
    if (mode > 1u)
    {
        mode = 1u;
    }

    if (m_ScanlineMode != mode)
    {
        m_ScanlineMode = mode;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetColorArtifactsEnabled(boolean enabled)
{
    enabled = enabled ? TRUE : FALSE;
    if (m_ColorArtifactsEnabled != enabled)
    {
        m_ColorArtifactsEnabled = enabled;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetGfx9000Enabled(boolean enabled)
{
    enabled = enabled ? TRUE : FALSE;
    if (m_Gfx9000Enabled != enabled)
    {
        m_Gfx9000Enabled = enabled;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetDiskRomEnabled(boolean enabled)
{
    enabled = enabled ? TRUE : FALSE;
    if (m_DiskRomEnabled != enabled)
    {
        m_DiskRomEnabled = enabled;
        m_ForceRedraw = TRUE;
    }
}

boolean CComboPauseMenu::IsDiskRomEnabled(void) const
{
    return m_DiskRomEnabled ? TRUE : FALSE;
}

void CComboPauseMenu::SetCoreName(const char *name)
{
    if (name == 0 || name[0] == '\0')
    {
        name = "sms";
    }
    if (!ComboTextEquals(m_CoreName, name))
    {
        CopySmallText(m_CoreName, sizeof(m_CoreName), name);
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetBootMode(unsigned mode)
{
    if (mode > 2u)
    {
        mode = 0u;
    }

    if (m_BootMode != mode)
    {
        m_BootMode = mode;
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetBootMode(void) const
{
    return m_BootMode;
}

void CComboPauseMenu::SetMachineProfile(unsigned profile)
{
    if (profile > 2u)
    {
        profile = 0u;
    }

    if (m_MachineProfile != profile)
    {
        m_MachineProfile = profile;
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetMachineProfile(void) const
{
    return m_MachineProfile;
}

void CComboPauseMenu::SetProcessorMode(unsigned mode)
{
    mode = mode == 1u ? 1u : 0u;
    if (m_ProcessorMode != mode)
    {
        m_ProcessorMode = mode;
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetProcessorMode(void) const
{
    return m_ProcessorMode;
}

void CComboPauseMenu::SetRamMapperKb(unsigned kb)
{
    if (kb != 128u && kb != 256u && kb != 512u)
    {
        kb = 512u;
    }
    if (m_RamMapperKb != kb)
    {
        m_RamMapperKb = kb;
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetRamMapperKb(void) const
{
    return m_RamMapperKb;
}

void CComboPauseMenu::SetMegaRamKb(unsigned kb)
{
    if (kb != 0u && kb != 256u && kb != 512u && kb != 1024u)
    {
        kb = 0u;
    }
    if (m_MegaRamKb != kb)
    {
        m_MegaRamKb = kb;
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetMegaRamKb(void) const
{
    return m_MegaRamKb;
}

void CComboPauseMenu::SetJoystickMapCodes(const u16 *codes, unsigned count)
{
    boolean changed = FALSE;

    for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
    {
        const u16 next = (codes != 0 && i < count) ? CUsbHidGamepad::ClampControlCode(codes[i]) : 0u;
        if (m_JoystickMapCodes[i] != next || m_JoystickMapEditCodes[i] != next)
        {
            m_JoystickMapCodes[i] = next;
            m_JoystickMapEditCodes[i] = next;
            changed = TRUE;
        }
    }

    if (changed)
    {
        m_JoystickMapDirty = FALSE;
        m_LastJoystickCaptureCode = 0u;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::GetJoystickMapCodes(u16 *codes, unsigned count) const
{
    if (codes == 0 || count == 0u)
    {
        return;
    }

    for (unsigned i = 0u; i < count; ++i)
    {
        codes[i] = (i < CUsbHidGamepad::MapSlotCount) ? m_JoystickMapCodes[i] : 0u;
    }
}

void CComboPauseMenu::SetFmMusicEnabled(boolean enabled)
{
    enabled = enabled ? TRUE : FALSE;
    if (m_FmMusicEnabled != enabled)
    {
        m_FmMusicEnabled = enabled;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetSccCartEnabled(boolean enabled)
{
    enabled = enabled ? TRUE : FALSE;
    if (m_SccCartEnabled != enabled)
    {
        m_SccCartEnabled = enabled;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetSccDualCartState(boolean enabled, boolean available)
{
    enabled = enabled ? TRUE : FALSE;
    available = available ? TRUE : FALSE;
    if (m_SccDualCartEnabled != enabled || m_SccDualCartAvailable != available)
    {
        m_SccDualCartEnabled = enabled;
        m_SccDualCartAvailable = available;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetAudioGainPercent(unsigned percent)
{
    if (percent > 100u)
    {
        percent = 100u;
    }
    if (m_AudioGainPercent != percent)
    {
        m_AudioGainPercent = percent;
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetLoadRomSlotNames(const char *slot1_name, const char *slot2_name)
{
    if (slot1_name == 0 || slot1_name[0] == '\0')
    {
        slot1_name = "";
    }
    if (slot2_name == 0 || slot2_name[0] == '\0')
    {
        slot2_name = "";
    }

    if (!ComboTextEquals(m_LoadRomSlot1Name, slot1_name) || !ComboTextEquals(m_LoadRomSlot2Name, slot2_name))
    {
        CopySmallText(m_LoadRomSlot1Name, sizeof(m_LoadRomSlot1Name), slot1_name);
        CopySmallText(m_LoadRomSlot2Name, sizeof(m_LoadRomSlot2Name), slot2_name);
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetLoadDiskDriveNames(const char *drive1_name, const char *drive2_name)
{
    if (drive1_name == 0 || drive1_name[0] == '\0')
    {
        drive1_name = "";
    }
    if (drive2_name == 0 || drive2_name[0] == '\0')
    {
        drive2_name = "";
    }

    if (!ComboTextEquals(m_LoadDiskDrive1Name, drive1_name) || !ComboTextEquals(m_LoadDiskDrive2Name, drive2_name))
    {
        CopySmallText(m_LoadDiskDrive1Name, sizeof(m_LoadDiskDrive1Name), drive1_name);
        CopySmallText(m_LoadDiskDrive2Name, sizeof(m_LoadDiskDrive2Name), drive2_name);
        m_ForceRedraw = TRUE;
    }
}

void CComboPauseMenu::SetLoadCassetteName(const char *cassette_name)
{
    if (cassette_name == 0 || cassette_name[0] == '\0')
    {
        cassette_name = "";
    }

    if (!ComboTextEquals(m_LoadCassetteName, cassette_name))
    {
        CopySmallText(m_LoadCassetteName, sizeof(m_LoadCassetteName), cassette_name);
        m_ForceRedraw = TRUE;
    }
}

unsigned CComboPauseMenu::GetAudioGainPercent(void) const
{
    return m_AudioGainPercent;
}

boolean CComboPauseMenu::ConsumePendingRomSelection(unsigned *slot_index, char *path, unsigned path_size)
{
    if (!m_HasPendingRomSelection)
    {
        return FALSE;
    }
    if (slot_index != 0)
    {
        *slot_index = m_PendingRomSlot;
    }
    if (path != 0 && path_size > 0u)
    {
        CopySmallText(path, path_size, m_PendingRomPath);
    }
    m_HasPendingRomSelection = FALSE;
    m_PendingRomSlot = 0u;
    CopySmallText(m_PendingRomPath, sizeof(m_PendingRomPath), "");
    return TRUE;
}

boolean CComboPauseMenu::ConsumePendingDiskSelection(unsigned *drive_index, char *path, unsigned path_size)
{
    if (!m_HasPendingDiskSelection)
    {
        return FALSE;
    }
    if (drive_index != 0)
    {
        *drive_index = m_PendingDiskDrive;
    }
    if (path != 0 && path_size > 0u)
    {
        CopySmallText(path, path_size, m_PendingDiskPath);
    }
    m_HasPendingDiskSelection = FALSE;
    m_PendingDiskDrive = 0u;
    CopySmallText(m_PendingDiskPath, sizeof(m_PendingDiskPath), "");
    return TRUE;
}

boolean CComboPauseMenu::ConsumePendingCassetteSelection(char *path, unsigned path_size)
{
    if (!m_HasPendingCassetteSelection)
    {
        return FALSE;
    }
    if (path != 0 && path_size > 0u)
    {
        CopySmallText(path, path_size, m_PendingCassettePath);
    }
    m_HasPendingCassetteSelection = FALSE;
    CopySmallText(m_PendingCassettePath, sizeof(m_PendingCassettePath), "");
    return TRUE;
}

boolean CComboPauseMenu::ConsumePendingRomBrowserOpen(unsigned *slot_index, char *path, unsigned path_size)
{
    if (!m_HasPendingRomBrowserOpen)
    {
        return FALSE;
    }
    if (slot_index != 0)
    {
        *slot_index = m_PendingRomBrowserSlot;
    }
    if (path != 0 && path_size > 0u)
    {
        CopySmallText(path, path_size, m_PendingRomBrowserPath);
    }
    m_HasPendingRomBrowserOpen = FALSE;
    m_PendingRomBrowserSlot = 0u;
    CopySmallText(m_PendingRomBrowserPath, sizeof(m_PendingRomBrowserPath), "");
    return TRUE;
}

boolean CComboPauseMenu::ConsumePendingDiskBrowserOpen(unsigned *drive_index, char *path, unsigned path_size)
{
    if (!m_HasPendingDiskBrowserOpen)
    {
        return FALSE;
    }
    if (drive_index != 0)
    {
        *drive_index = m_PendingDiskBrowserDrive;
    }
    if (path != 0 && path_size > 0u)
    {
        CopySmallText(path, path_size, m_PendingDiskBrowserPath);
    }
    m_HasPendingDiskBrowserOpen = FALSE;
    m_PendingDiskBrowserDrive = 0u;
    CopySmallText(m_PendingDiskBrowserPath, sizeof(m_PendingDiskBrowserPath), "");
    return TRUE;
}

boolean CComboPauseMenu::ConsumePendingCassetteBrowserOpen(char *path, unsigned path_size)
{
    if (!m_HasPendingCassetteBrowserOpen)
    {
        return FALSE;
    }
    if (path != 0 && path_size > 0u)
    {
        CopySmallText(path, path_size, m_PendingCassetteBrowserPath);
    }
    m_HasPendingCassetteBrowserOpen = FALSE;
    CopySmallText(m_PendingCassetteBrowserPath, sizeof(m_PendingCassetteBrowserPath), "");
    return TRUE;
}

void CComboPauseMenu::OpenFileBrowser(unsigned mode, unsigned slot_index, const char *path)
{
    if (path != 0 && path[0] != '\0')
    {
        m_FileBrowser.EnterDirectory(path);
    }
    else
    {
        m_FileBrowser.Open(mode, slot_index);
    }
    m_View = (mode == (unsigned) ComboFileBrowserModeDisk)
        ? MenuViewLoadDiskBrowser
        : ((mode == (unsigned) ComboFileBrowserModeCassette) ? MenuViewLoadCassetteBrowser : MenuViewLoadRomBrowser);
    m_Selected = 0u;
    m_ForceRedraw = TRUE;
    m_ForceHighlightRedraw = FALSE;
    m_RequestBackgroundRestore = TRUE;
    m_BrowserWarmupRedraws = 1u;
}

boolean CComboPauseMenu::IsInSettings(void) const
{
    return (m_View != MenuViewRoot) ? TRUE : FALSE;
}

boolean CComboPauseMenu::IsCapturingJoystickMap(void) const
{
    return (m_View == MenuViewJoystickMap) ? TRUE : FALSE;
}

void CComboPauseMenu::ReturnToRoot(void)
{
    const unsigned old_view = m_View;
    m_View = MenuViewRoot;
    m_Selected = FindFirstSelectable();
    m_JoystickMapDirty = FALSE;
    m_LastJoystickCaptureCode = 0u;
    for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
    {
        m_JoystickMapEditCodes[i] = m_JoystickMapCodes[i];
    }
    const boolean silent_settings_back = (old_view == MenuViewSettings || old_view == MenuViewJoystickMap) ? TRUE : FALSE;
    m_ForceRedraw = silent_settings_back ? FALSE : TRUE;
    m_ForceHighlightRedraw = FALSE;
    m_ForceContentRedraw = FALSE;
    m_SuppressNextRender = silent_settings_back;
    m_RequestBackgroundRestore = silent_settings_back ? FALSE : TRUE;
}

void CComboPauseMenu::RequestRedraw(void)
{
    m_ForceRedraw = TRUE;
}

void CComboPauseMenu::InvalidateViewportSnapshot(void)
{
    /* Snapshot buffer is global/fixed and intentionally persistent until overwritten. */
}

void CComboPauseMenu::CaptureViewportSnapshot(CGfxTextBoxRenderer *renderer, unsigned x, unsigned y, unsigned w, unsigned h)
{
    combo_viewport_screenshot_capture(renderer, x, y, w, h);
}

boolean CComboPauseMenu::GetViewportSnapshotView(const TScreenColor **pixels, unsigned *x, unsigned *y, unsigned *w, unsigned *h, unsigned *stride) const
{
    TViewportScreenshotView view;
    if (pixels != 0)
    {
        *pixels = 0;
    }
    if (x != 0) *x = 0u;
    if (y != 0) *y = 0u;
    if (w != 0) *w = 0u;
    if (h != 0) *h = 0u;
    if (stride != 0) *stride = 0u;

    if (!combo_viewport_screenshot_get(&view))
    {
        return FALSE;
    }

    if (pixels != 0) *pixels = view.pixels;
    if (x != 0) *x = view.x;
    if (y != 0) *y = view.y;
    if (w != 0) *w = view.w;
    if (h != 0) *h = view.h;
    if (stride != 0) *stride = view.stride;
    return TRUE;
}

boolean CComboPauseMenu::GetOffscreenView(const TScreenColor **pixels, unsigned *x, unsigned *y, unsigned *w, unsigned *h, unsigned *stride) const
{
    if (pixels != 0)
    {
        *pixels = 0;
    }
    if (x != 0) *x = 0u;
    if (y != 0) *y = 0u;
    if (w != 0) *w = 0u;
    if (h != 0) *h = 0u;
    if (stride != 0) *stride = 0u;

    if (m_MenuBackbuffer == 0 || m_MenuBackbufferW == 0u || m_MenuBackbufferH == 0u)
    {
        return FALSE;
    }

    if (pixels != 0) *pixels = m_MenuBackbuffer;
    if (x != 0) *x = m_MenuBackbufferX;
    if (y != 0) *y = m_MenuBackbufferY;
    if (w != 0) *w = m_MenuBackbufferW;
    if (h != 0) *h = m_MenuBackbufferH;
    if (stride != 0) *stride = m_MenuBackbufferW;
    return TRUE;
}

boolean CComboPauseMenu::ConsumeBackgroundRestoreRequest(void)
{
    const boolean pending = m_RequestBackgroundRestore;
    m_RequestBackgroundRestore = FALSE;
    return pending;
}

const TComboMenuItem *CComboPauseMenu::GetItems(unsigned *count) const
{
    if (m_View == MenuViewSettings)
    {
        return combo_menu_settings_items_get(
            m_Language,
            m_ScalePercent,
            m_ScanlineMode,
            m_ColorArtifactsEnabled,
            m_Gfx9000Enabled,
            m_DiskRomEnabled,
            m_CoreName,
            m_MachineProfile,
            m_ProcessorMode,
            m_RamMapperKb,
            m_MegaRamKb,
            m_FmMusicEnabled,
            m_SccCartEnabled,
            m_SccDualCartEnabled,
            m_SccDualCartAvailable,
            m_AudioGainPercent,
            count);
    }
    if (m_View == MenuViewJoystickMap)
    {
        static char code_labels[CUsbHidGamepad::MapSlotCount][16];

        for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
        {
            CUsbHidGamepad::FormatControlCode(m_JoystickMapEditCodes[i], code_labels[i], sizeof(code_labels[i]));
        }

        return combo_menu_joystick_map_items_get(
            m_Language,
            code_labels[CUsbHidGamepad::MapSlotUp],
            code_labels[CUsbHidGamepad::MapSlotDown],
            code_labels[CUsbHidGamepad::MapSlotLeft],
            code_labels[CUsbHidGamepad::MapSlotRight],
            code_labels[CUsbHidGamepad::MapSlotButton1],
            code_labels[CUsbHidGamepad::MapSlotButton2],
            count);
    }
    if (m_View == MenuViewLoadRom)
    {
        return combo_menu_load_rom_items_get(m_Language, m_LoadRomSlot1Name, m_LoadRomSlot2Name, count);
    }
    if (m_View == MenuViewLoadDisk)
    {
        return combo_menu_load_disk_items_get(m_Language, m_LoadDiskDrive1Name, m_LoadDiskDrive2Name, count);
    }
    if (m_View == MenuViewLoadCassette)
    {
        return combo_menu_load_cassette_items_get(m_Language, m_LoadCassetteName, count);
    }
    if (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser)
    {
        const TComboLocaleStrings *locale = combo_locale_get(m_Language);
        if (locale == 0)
        {
            locale = combo_locale_get(ComboLanguageEN);
        }

        const char *empty = (m_View == MenuViewLoadDiskBrowser)
            ? locale->load_disk_empty
            : ((m_View == MenuViewLoadCassetteBrowser) ? locale->load_cassette_empty : locale->load_rom_empty);
        if (empty == 0 || empty[0] == '\0')
        {
            empty = "EMPTY";
        }

        if (m_FileBrowser.GetCount() == 0u)
        {
            CopySmallText(m_RomBrowserEmptyLabel, sizeof(m_RomBrowserEmptyLabel), empty);
            m_RomBrowserItems[0].label = m_RomBrowserEmptyLabel;
            m_RomBrowserItems[0].action = ComboMenuActionNone;
            m_RomBrowserItems[0].selectable = FALSE;
            m_RomBrowserItems[0].redraw_policy = ComboMenuRedrawMenuOnly;
            if (count != 0)
            {
                *count = 1u;
            }
            return m_RomBrowserItems;
        }

        const unsigned visible = m_FileBrowser.GetVisibleCount();
        for (unsigned i = 0u; i < visible; ++i)
        {
            CopySmallText(m_RomBrowserItemLabels[i], sizeof(m_RomBrowserItemLabels[i]), m_FileBrowser.GetLabelVisible(i));
            m_RomBrowserItems[i].label = m_RomBrowserItemLabels[i];
            m_RomBrowserItems[i].action = ComboMenuActionNone;
            m_RomBrowserItems[i].selectable = TRUE;
            m_RomBrowserItems[i].redraw_policy = ComboMenuRedrawMenuOnly;
        }
        if (count != 0)
        {
            *count = visible;
        }
        return m_RomBrowserItems;
    }

    return combo_menu_root_items_get(m_Language, m_BootMode, count);
}

unsigned CComboPauseMenu::FindFirstSelectable(void) const
{
    unsigned item_count = 0u;
    const TComboMenuItem *items = GetItems(&item_count);

    for (unsigned i = 0; i < item_count; ++i)
    {
        if (items[i].selectable)
        {
            return i;
        }
    }

    return 0u;
}

unsigned CComboPauseMenu::FindActionIndex(TComboMenuAction action) const
{
    unsigned item_count = 0u;
    const TComboMenuItem *items = GetItems(&item_count);

    for (unsigned i = 0u; i < item_count; ++i)
    {
        if (items[i].action == action)
        {
            return i;
        }
    }

    return FindFirstSelectable();
}

unsigned CComboPauseMenu::FindPrevSelectable(unsigned from) const
{
    unsigned item_count = 0u;
    const TComboMenuItem *items = GetItems(&item_count);
    if (item_count == 0u)
    {
        return 0u;
    }

    for (unsigned step = 0u; step < item_count; ++step)
    {
        from = (from == 0u) ? (item_count - 1u) : (from - 1u);
        if (items[from].selectable)
        {
            return from;
        }
    }

    return from;
}

unsigned CComboPauseMenu::FindNextSelectable(unsigned from) const
{
    unsigned item_count = 0u;
    const TComboMenuItem *items = GetItems(&item_count);
    if (item_count == 0u)
    {
        return 0u;
    }

    for (unsigned step = 0u; step < item_count; ++step)
    {
        from = (from + 1u) % item_count;
        if (items[from].selectable)
        {
            return from;
        }
    }

    return from;
}

TComboMenuAction CComboPauseMenu::ProcessInput(boolean paused,
                                               boolean up_pressed,
                                               boolean down_pressed,
                                               boolean page_up_pressed,
                                               boolean page_down_pressed,
                                               boolean left_pressed,
                                               boolean right_pressed,
                                               boolean enter_pressed,
                                               boolean back_pressed,
                                               boolean delete_pressed,
                                               char jump_letter,
                                               u16 joystick_capture_code)
{
    const unsigned old_view = m_View;
    const unsigned old_selected = m_Selected;

    if (!paused)
    {
        ResetRepeatState();
        m_LastBackPressed = back_pressed;
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_LastLeftPressed = left_pressed;
        m_LastRightPressed = right_pressed;
        m_LastEnterPressed = enter_pressed;
        m_LastDeletePressed = delete_pressed;
        return ComboMenuActionNone;
    }

    unsigned item_count = 0u;
    const TComboMenuItem *items = GetItems(&item_count);
    if (item_count == 0u)
    {
        ResetRepeatState();
        m_LastBackPressed = back_pressed;
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_LastLeftPressed = left_pressed;
        m_LastRightPressed = right_pressed;
        m_LastEnterPressed = enter_pressed;
        m_LastDeletePressed = delete_pressed;
        return ComboMenuActionNone;
    }

    TComboMenuAction action = ComboMenuActionNone;
    const boolean joystick_capture_edge =
        (joystick_capture_code != 0u && joystick_capture_code != m_LastJoystickCaptureCode) ? TRUE : FALSE;

    if (back_pressed && !m_LastBackPressed && m_View == MenuViewJoystickMap)
    {
        action = m_JoystickMapDirty ? ComboMenuActionApplyJoystickMap : ComboMenuActionNone;
        for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
        {
            m_JoystickMapCodes[i] = m_JoystickMapEditCodes[i];
        }
        m_View = MenuViewSettings;
        m_Selected = m_JoystickMapReturnSelected;
        m_JoystickMapDirty = FALSE;
        m_LastJoystickCaptureCode = 0u;
        m_ForceRedraw = TRUE;
        m_ForceHighlightRedraw = FALSE;
        m_ForceContentRedraw = FALSE;
        m_RequestBackgroundRestore = TRUE;
        ResetRepeatState();
        m_LastBackPressed = back_pressed;
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_LastLeftPressed = left_pressed;
        m_LastRightPressed = right_pressed;
        m_LastEnterPressed = enter_pressed;
        m_LastDeletePressed = delete_pressed;
        return action;
    }

    if (back_pressed && !m_LastBackPressed && m_View != MenuViewRoot)
    {
        if (m_View == MenuViewLoadRomBrowser)
        {
            m_View = MenuViewRoot;
        }
        else if (m_View == MenuViewLoadDiskBrowser)
        {
            m_View = MenuViewLoadDisk;
        }
        else if (m_View == MenuViewLoadCassetteBrowser)
        {
            m_View = MenuViewLoadCassette;
        }
        else
        {
            m_View = MenuViewRoot;
        }
        m_Selected = FindFirstSelectable();
        const boolean silent_settings_back = (m_View == MenuViewRoot && old_view == MenuViewSettings) ? TRUE : FALSE;
        m_ForceRedraw = silent_settings_back ? FALSE : TRUE;
        m_ForceHighlightRedraw = FALSE;
        m_ForceContentRedraw = FALSE;
        m_SuppressNextRender = silent_settings_back;
        m_RequestBackgroundRestore = silent_settings_back ? FALSE : TRUE;
        ResetRepeatState();
        m_LastBackPressed = back_pressed;
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_LastLeftPressed = left_pressed;
        m_LastRightPressed = right_pressed;
        m_LastEnterPressed = enter_pressed;
        m_LastDeletePressed = delete_pressed;
        return ComboMenuActionNone;
    }

    if (m_View == MenuViewJoystickMap)
    {
        if (m_Selected >= CUsbHidGamepad::MapSlotCount)
        {
            m_Selected = 0u;
        }

        if (joystick_capture_edge)
        {
            joystick_capture_code = CUsbHidGamepad::ClampControlCode(joystick_capture_code);
            boolean changed = FALSE;
            for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
            {
                if (i != m_Selected && m_JoystickMapEditCodes[i] == joystick_capture_code)
                {
                    m_JoystickMapEditCodes[i] = 0u;
                    changed = TRUE;
                }
            }
            if (m_JoystickMapEditCodes[m_Selected] != joystick_capture_code)
            {
                m_JoystickMapEditCodes[m_Selected] = joystick_capture_code;
                changed = TRUE;
            }
            if (changed)
            {
                for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
                {
                    m_JoystickMapCodes[i] = m_JoystickMapEditCodes[i];
                }
            }
            m_JoystickMapDirty = TRUE;
            m_Selected = (m_Selected + 1u) % CUsbHidGamepad::MapSlotCount;
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_ForceContentRedraw = FALSE;
        }

        m_LastJoystickCaptureCode = joystick_capture_code;
        ResetRepeatState();
        m_LastBackPressed = back_pressed;
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_LastLeftPressed = left_pressed;
        m_LastRightPressed = right_pressed;
        m_LastEnterPressed = enter_pressed;
        m_LastDeletePressed = delete_pressed;
        return ComboMenuActionNone;
    }

    if (m_Selected >= item_count || !items[m_Selected].selectable)
    {
        m_Selected = FindFirstSelectable();
    }

    m_LastJoystickCaptureCode = joystick_capture_code;

    if (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser)
    {
        ResetRepeatState();
        const unsigned old_browser_top = m_FileBrowser.GetTop();
        const unsigned old_browser_abs = m_FileBrowser.GetSelectedAbs();
        const unsigned old_browser_count = m_FileBrowser.GetCount();
        const boolean letter_edge = (jump_letter >= 'A' && jump_letter <= 'Z') ? TRUE : FALSE;
        const boolean page_nav_edge =
            ((page_up_pressed && !m_LastPageUpPressed)
             || (page_down_pressed && !m_LastPageDownPressed)) ? TRUE : FALSE;
        const boolean nav_edge =
            ((up_pressed && !m_LastUpPressed)
             || (down_pressed && !m_LastDownPressed)
             || (page_up_pressed && !m_LastPageUpPressed)
             || (page_down_pressed && !m_LastPageDownPressed)
             || letter_edge) ? TRUE : FALSE;
        char selected_path[192];
        selected_path[0] = '\0';
        const TComboFileBrowserEvent browser_event = m_FileBrowser.ProcessInput(
            up_pressed,
            down_pressed,
            page_up_pressed,
            page_down_pressed,
            enter_pressed && !m_LastEnterPressed,
            selected_path,
            sizeof(selected_path));

        boolean letter_jump_moved = FALSE;
        if (letter_edge)
        {
            const unsigned before_jump_top = m_FileBrowser.GetTop();
            const unsigned before_jump_abs = m_FileBrowser.GetSelectedAbs();
            if (m_FileBrowser.JumpToFirstContaining(jump_letter))
            {
                const unsigned after_jump_top = m_FileBrowser.GetTop();
                const unsigned after_jump_abs = m_FileBrowser.GetSelectedAbs();
                letter_jump_moved = (before_jump_top != after_jump_top || before_jump_abs != after_jump_abs) ? TRUE : FALSE;
                if (letter_jump_moved)
                {
                    /* Same visual policy as cursor navigation, but with one extra settle pass. */
                    m_LastDrawnBrowserTop = 0xFFFFFFFFu;
                    m_LastDrawnBrowserSelectedAbs = 0xFFFFFFFFu;
                    m_LastDrawnBrowserContentStamp = 0xFFFFFFFFu;
                    m_BrowserWarmupRedraws = 1u;
                }
            }
        }

        const unsigned new_browser_top = m_FileBrowser.GetTop();
        const unsigned new_browser_abs = m_FileBrowser.GetSelectedAbs();
        const unsigned new_browser_count = m_FileBrowser.GetCount();
        const boolean browser_scrolled_or_moved =
            (old_browser_top != new_browser_top
             || old_browser_abs != new_browser_abs
             || old_browser_count != new_browser_count) ? TRUE : FALSE;
        if (page_nav_edge && browser_scrolled_or_moved)
        {
            /* Same anti-glitch settle pass used for A..Z jump. */
            m_LastDrawnBrowserTop = 0xFFFFFFFFu;
            m_LastDrawnBrowserSelectedAbs = 0xFFFFFFFFu;
            m_LastDrawnBrowserContentStamp = 0xFFFFFFFFu;
            m_BrowserWarmupRedraws = 1u;
        }

        if (browser_event == ComboFileBrowserEventEnteredDirectory)
        {
            if (selected_path[0] != '\0')
            {
                if (m_FileBrowser.GetMode() == (unsigned) ComboFileBrowserModeRom)
                {
                    m_HasPendingRomBrowserOpen = TRUE;
                    m_PendingRomBrowserSlot = m_FileBrowser.GetSlotIndex();
                    CopySmallText(m_PendingRomBrowserPath, sizeof(m_PendingRomBrowserPath), selected_path);
                    action = (m_PendingRomBrowserSlot == 0u) ? ComboMenuActionCartridgeA : ComboMenuActionCartridgeB;
                }
                else if (m_FileBrowser.GetMode() == (unsigned) ComboFileBrowserModeDisk)
                {
                    m_HasPendingDiskBrowserOpen = TRUE;
                    m_PendingDiskBrowserDrive = m_FileBrowser.GetSlotIndex();
                    CopySmallText(m_PendingDiskBrowserPath, sizeof(m_PendingDiskBrowserPath), selected_path);
                    action = (m_PendingDiskBrowserDrive == 0u) ? ComboMenuActionDiskDriveA : ComboMenuActionDiskDriveB;
                }
                else if (m_FileBrowser.GetMode() == (unsigned) ComboFileBrowserModeCassette)
                {
                    m_HasPendingCassetteBrowserOpen = TRUE;
                    CopySmallText(m_PendingCassetteBrowserPath, sizeof(m_PendingCassetteBrowserPath), selected_path);
                    action = ComboMenuActionCassette;
                }
            }
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_ForceContentRedraw = FALSE;
            m_LastDrawnSelected = 0xFFFFFFFFu;
            m_LastDrawnBrowserTop = 0xFFFFFFFFu;
            m_LastDrawnBrowserCount = 0xFFFFFFFFu;
            m_LastDrawnBrowserSelectedAbs = 0xFFFFFFFFu;
            m_LastDrawnBrowserMode = 0xFFFFFFFFu;
            m_LastDrawnBrowserSlot = 0xFFFFFFFFu;
            m_LastDrawnBrowserContentStamp = 0xFFFFFFFFu;
            m_LastBoxValid = FALSE;
            for (unsigned i = 0u; i < CComboFileBrowser::kVisibleRows; ++i)
            {
                m_RomBrowserItemLabels[i][0] = '\0';
                m_RomBrowserItems[i].label = m_RomBrowserItemLabels[i];
                m_RomBrowserItems[i].action = ComboMenuActionNone;
                m_RomBrowserItems[i].selectable = FALSE;
                m_RomBrowserItems[i].redraw_policy = ComboMenuRedrawMenuOnly;
            }
            /* Directory changes can alter box geometry; restore viewport first to avoid stale menu fragments. */
            m_RequestBackgroundRestore = TRUE;
            m_BrowserWarmupRedraws = 1u; /* extra pass to settle freshly-entered directory */
        }
        else if (browser_scrolled_or_moved
              || nav_edge
              || letter_jump_moved
              || browser_event == ComboFileBrowserEventSelectionChanged)
        {
            m_ForceContentRedraw = TRUE;
        }

        m_Selected = m_FileBrowser.GetSelectedVisibleIndex();
        if (browser_event == ComboFileBrowserEventSelectedFile && selected_path[0] != '\0')
        {
            if (m_FileBrowser.GetMode() == (unsigned) ComboFileBrowserModeRom)
            {
                m_HasPendingRomSelection = TRUE;
                m_PendingRomSlot = 0u;
                CopySmallText(m_PendingRomPath, sizeof(m_PendingRomPath), selected_path);
                action = (m_PendingRomSlot == 0u) ? ComboMenuActionCartridgeA : ComboMenuActionCartridgeB;
                m_View = MenuViewRoot;
                m_Selected = FindFirstSelectable();
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
                m_ForceContentRedraw = FALSE;
                m_RequestBackgroundRestore = TRUE;
            }
            else if (m_FileBrowser.GetMode() == (unsigned) ComboFileBrowserModeDisk)
            {
                m_HasPendingDiskSelection = TRUE;
                m_PendingDiskDrive = m_FileBrowser.GetSlotIndex();
                CopySmallText(m_PendingDiskPath, sizeof(m_PendingDiskPath), selected_path);
                action = (m_PendingDiskDrive == 0u) ? ComboMenuActionDiskDriveA : ComboMenuActionDiskDriveB;
                m_View = MenuViewLoadDisk;
                m_Selected = (m_PendingDiskDrive == 0u) ? 0u : 1u;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
                m_ForceContentRedraw = FALSE;
                m_RequestBackgroundRestore = TRUE;
            }
            else if (m_FileBrowser.GetMode() == (unsigned) ComboFileBrowserModeCassette)
            {
                m_HasPendingCassetteSelection = TRUE;
                CopySmallText(m_PendingCassettePath, sizeof(m_PendingCassettePath), selected_path);
                action = ComboMenuActionCassette;
                m_View = MenuViewLoadCassette;
                m_Selected = 0u;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
                m_ForceContentRedraw = FALSE;
                m_RequestBackgroundRestore = TRUE;
            }
        }

        if (m_View != old_view)
        {
            ResetRepeatState();
        }
        m_LastBackPressed = back_pressed;
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_LastLeftPressed = left_pressed;
        m_LastRightPressed = right_pressed;
        m_LastEnterPressed = enter_pressed;
        m_LastDeletePressed = delete_pressed;

        const boolean old_browser = (old_view == MenuViewLoadRomBrowser || old_view == MenuViewLoadDiskBrowser || old_view == MenuViewLoadCassetteBrowser) ? TRUE : FALSE;
        const boolean new_browser = (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser) ? TRUE : FALSE;
        if (old_browser != new_browser)
        {
            m_RequestBackgroundRestore = TRUE;
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_ForceContentRedraw = FALSE;
        }
        return action;
    }

    const boolean up_step = ComboConsumeHoldRepeat(up_pressed, m_LastUpPressed, &m_UpHoldFrames);
    const boolean down_step = ComboConsumeHoldRepeat(down_pressed, m_LastDownPressed, &m_DownHoldFrames);

    if (up_step)
    {
        m_Selected = FindPrevSelectable(m_Selected);
    }

    if (down_step)
    {
        m_Selected = FindNextSelectable(m_Selected);
    }

    boolean handled_settings_input = FALSE;
    if (m_View == MenuViewSettings && items[m_Selected].selectable)
    {
        const boolean enter_edge = enter_pressed && !m_LastEnterPressed;
        const boolean left_step = ComboConsumeHoldRepeat(left_pressed, m_LastLeftPressed, &m_LeftHoldFrames);
        const boolean right_step = ComboConsumeHoldRepeat(right_pressed, m_LastRightPressed, &m_RightHoldFrames);

        if (items[m_Selected].action == ComboMenuActionOverscanChanged)
        {
            unsigned next_value = m_ScalePercent;
            if (left_step)
            {
                if (next_value > 250u)
                {
                    next_value = 250u;
                }
                else if (next_value > 200u)
                {
                    next_value = 200u;
                }
            }
            if (right_step || enter_edge)
            {
                if (next_value < 250u)
                {
                    next_value = 250u;
                }
                else if (next_value < 300u)
                {
                    next_value = 300u;
                }
            }

            if (next_value != m_ScalePercent)
            {
                m_ScalePercent = next_value;
                m_ScaleMaxFitEnabled = FALSE;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
                action = ComboMenuActionOverscanChanged;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleScanlines)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleScanlinesPrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleScanlines;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionToggleColorArtifacts)
        {
            if (enter_edge || left_step || right_step)
            {
                action = ComboMenuActionToggleColorArtifacts;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleMachine)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleMachinePrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleMachine;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleCore)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleCorePrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleCore;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleProcessor)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleProcessorPrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleProcessor;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleRamMapper)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleRamMapperPrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleRamMapper;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleMegaRam)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleMegaRamPrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleMegaRam;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionCycleLanguage)
        {
            if (left_step)
            {
                action = ComboMenuActionCycleLanguagePrev;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            else if (right_step || enter_edge)
            {
                action = ComboMenuActionCycleLanguage;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if ((items[m_Selected].action == ComboMenuActionToggleFmMusic)
              || (items[m_Selected].action == ComboMenuActionToggleColorArtifacts)
              || (items[m_Selected].action == ComboMenuActionToggleGfx9000)
              || (items[m_Selected].action == ComboMenuActionToggleDiskRom)
              || (items[m_Selected].action == ComboMenuActionToggleSccCart))
        {
            if (enter_edge || left_step || right_step)
            {
                action = items[m_Selected].action;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
        else if (items[m_Selected].action == ComboMenuActionAudioGainChanged)
        {
            unsigned next_level = AudioGainUiLevelFromPercent(m_AudioGainPercent);
            if (left_step)
            {
                next_level = (next_level == 0u) ? 9u : (next_level - 1u);
            }
            if (right_step || enter_edge)
            {
                next_level = (next_level >= 9u) ? 0u : (next_level + 1u);
            }
            const unsigned next_value = AudioGainPercentFromUiLevel(next_level);
            if (next_value != m_AudioGainPercent)
            {
                m_AudioGainPercent = next_value;
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
                action = ComboMenuActionAudioGainChanged;
            }
            handled_settings_input = enter_edge || left_step || right_step;
        }
    }

    if (enter_pressed && !m_LastEnterPressed && items[m_Selected].selectable && !handled_settings_input)
    {
        if (m_View == MenuViewRoot && items[m_Selected].action == ComboMenuActionSettings)
        {
            m_View = MenuViewSettings;
            m_Selected = FindFirstSelectable();
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_RequestBackgroundRestore = TRUE;
        }
        else if (m_View == MenuViewRoot && items[m_Selected].action == ComboMenuActionCartridgeA)
        {
            m_HasPendingRomBrowserOpen = TRUE;
            m_PendingRomBrowserSlot = 0u;
            CopySmallText(m_PendingRomBrowserPath, sizeof(m_PendingRomBrowserPath), "");
            action = ComboMenuActionCartridgeA;
        }
        else if (m_View == MenuViewRoot && items[m_Selected].action == ComboMenuActionDiskDriveA)
        {
            m_View = MenuViewLoadDisk;
            m_Selected = FindFirstSelectable();
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_RequestBackgroundRestore = TRUE;
        }
        else if (m_View == MenuViewRoot && items[m_Selected].action == ComboMenuActionCassette)
        {
            m_View = MenuViewLoadCassette;
            m_Selected = FindFirstSelectable();
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_RequestBackgroundRestore = TRUE;
        }
        else if (m_View == MenuViewSettings && items[m_Selected].action == ComboMenuActionOpenJoystickMap)
        {
            m_JoystickMapReturnSelected = m_Selected;
            for (unsigned i = 0u; i < CUsbHidGamepad::MapSlotCount; ++i)
            {
                m_JoystickMapEditCodes[i] = m_JoystickMapCodes[i];
            }
            m_JoystickMapDirty = FALSE;
            m_LastJoystickCaptureCode = joystick_capture_code;
            m_View = MenuViewJoystickMap;
            m_Selected = FindFirstSelectable();
            m_ForceRedraw = TRUE;
            m_ForceHighlightRedraw = FALSE;
            m_RequestBackgroundRestore = TRUE;
        }
        else if (m_View == MenuViewLoadRom
              && (items[m_Selected].action == ComboMenuActionCartridgeA || items[m_Selected].action == ComboMenuActionCartridgeB))
        {
            const unsigned slot = (items[m_Selected].action == ComboMenuActionCartridgeA) ? 0u : 1u;
            m_HasPendingRomBrowserOpen = TRUE;
            m_PendingRomBrowserSlot = slot;
            CopySmallText(m_PendingRomBrowserPath, sizeof(m_PendingRomBrowserPath), "");
            action = items[m_Selected].action;
        }
        else if (m_View == MenuViewLoadDisk
              && (items[m_Selected].action == ComboMenuActionDiskDriveA || items[m_Selected].action == ComboMenuActionDiskDriveB))
        {
            const unsigned slot = (items[m_Selected].action == ComboMenuActionDiskDriveA) ? 0u : 1u;
            m_HasPendingDiskBrowserOpen = TRUE;
            m_PendingDiskBrowserDrive = slot;
            CopySmallText(m_PendingDiskBrowserPath, sizeof(m_PendingDiskBrowserPath), "");
            action = items[m_Selected].action;
        }
        else if (m_View == MenuViewLoadCassette && items[m_Selected].action == ComboMenuActionCassette)
        {
            m_HasPendingCassetteBrowserOpen = TRUE;
            CopySmallText(m_PendingCassetteBrowserPath, sizeof(m_PendingCassetteBrowserPath), "");
            action = ComboMenuActionCassette;
        }
        else
        {
            action = items[m_Selected].action;
            if (m_View == MenuViewSettings)
            {
                /* Force redraw of dynamic labels (e.g. [LIG]/[DES]) immediately on Enter. */
                m_ForceRedraw = TRUE;
                m_ForceHighlightRedraw = FALSE;
            }
        }
    }

    if (delete_pressed && !m_LastDeletePressed && items[m_Selected].selectable)
    {
        if (m_View == MenuViewLoadRom
              && (items[m_Selected].action == ComboMenuActionCartridgeA || items[m_Selected].action == ComboMenuActionCartridgeB))
        {
            m_HasPendingRomSelection = TRUE;
            m_PendingRomSlot = (items[m_Selected].action == ComboMenuActionCartridgeA) ? 0u : 1u;
            CopySmallText(m_PendingRomPath, sizeof(m_PendingRomPath), "");
            action = items[m_Selected].action;
        }
        else if (m_View == MenuViewLoadDisk
              && (items[m_Selected].action == ComboMenuActionDiskDriveA || items[m_Selected].action == ComboMenuActionDiskDriveB))
        {
            m_HasPendingDiskSelection = TRUE;
            m_PendingDiskDrive = (items[m_Selected].action == ComboMenuActionDiskDriveA) ? 0u : 1u;
            CopySmallText(m_PendingDiskPath, sizeof(m_PendingDiskPath), "");
            action = items[m_Selected].action;
        }
        else if (m_View == MenuViewLoadCassette && items[m_Selected].action == ComboMenuActionCassette)
        {
            m_HasPendingCassetteSelection = TRUE;
            CopySmallText(m_PendingCassettePath, sizeof(m_PendingCassettePath), "");
            action = ComboMenuActionCassette;
        }
    }

    if (m_View != old_view)
    {
        ResetRepeatState();
    }
    m_LastBackPressed = back_pressed;
    m_LastUpPressed = up_pressed;
    m_LastDownPressed = down_pressed;
    m_LastPageUpPressed = page_up_pressed;
    m_LastPageDownPressed = page_down_pressed;
    m_LastLeftPressed = left_pressed;
    m_LastRightPressed = right_pressed;
    m_LastEnterPressed = enter_pressed;
    m_LastDeletePressed = delete_pressed;

    const boolean old_browser = (old_view == MenuViewLoadRomBrowser || old_view == MenuViewLoadDiskBrowser || old_view == MenuViewLoadCassetteBrowser) ? TRUE : FALSE;
    const boolean new_browser = (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser) ? TRUE : FALSE;
    if (old_browser != new_browser)
    {
        m_RequestBackgroundRestore = TRUE;
        m_ForceRedraw = TRUE;
        m_ForceHighlightRedraw = FALSE;
    }
    else if (m_View == MenuViewSettings
          && action != ComboMenuActionNone
          && m_Selected < item_count
          && items[m_Selected].redraw_policy == ComboMenuRedrawMenuAndViewport)
    {
        m_RequestBackgroundRestore = TRUE;
    }
    else if (!new_browser
          && !m_ForceRedraw
          && m_Selected != old_selected)
    {
        m_ForceHighlightRedraw = TRUE;
    }

    return action;
}

void CComboPauseMenu::Render(boolean visible, CGfxTextBoxRenderer *renderer, unsigned screen_width, unsigned screen_height,
                             unsigned anchor_x, unsigned anchor_y, unsigned anchor_w, unsigned anchor_h)
{
    const TGfxTextStyle title_style = { kPauseMenuFgColor, kPauseMenuBgColor, TRUE };
    const TGfxTextStyle normal_style = { kPauseMenuFgColor, kPauseMenuBgColor, FALSE };
    const TGfxTextStyle selected_style = { kPauseMenuBgColor, kPauseMenuFgColor, FALSE };
    const unsigned scale_x = 1u;
    const unsigned scale_y = 1u;

    if (renderer == 0)
    {
        return;
    }

    if (!visible)
    {
        m_Visible = FALSE;
        m_LastDrawnSelected = 0xFFFFFFFFu;
        m_ForceHighlightRedraw = FALSE;
        m_ForceContentRedraw = FALSE;
        m_MenuBackbufferW = 0u;
        m_MenuBackbufferH = 0u;
        m_MenuBackbufferX = 0u;
        m_MenuBackbufferY = 0u;
        m_BrowserWarmupRedraws = 0u;
        ComboClearPixels(m_MenuBackbuffer, m_MenuBackbufferCapacityPixels);
        return;
    }

    if (m_SuppressNextRender)
    {
        m_SuppressNextRender = FALSE;
        return;
    }

    unsigned item_count = 0u;
    const TComboMenuItem *items = GetItems(&item_count);
    if (item_count == 0u)
    {
        return;
    }

    const TComboLocaleStrings *locale = combo_locale_get(m_Language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }
    const char *title = locale->title_pause;
    if (m_View == MenuViewSettings)
    {
        title = locale->title_settings;
    }
    else if (m_View == MenuViewJoystickMap)
    {
        title = locale->title_joystick_map;
    }
    else if (m_View == MenuViewLoadRom)
    {
        title = locale->title_load_rom;
    }
    else if (m_View == MenuViewLoadDisk)
    {
        title = locale->title_load_disk;
    }
    else if (m_View == MenuViewLoadCassette)
    {
        title = locale->title_load_cassette;
    }
    else if (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser)
    {
        if (m_View == MenuViewLoadRomBrowser)
        {
            title = locale->title_load_rom;
        }
        else
        {
        const boolean disk_browser = (m_View == MenuViewLoadDiskBrowser) ? TRUE : FALSE;
        const boolean cassette_browser = (m_View == MenuViewLoadCassetteBrowser) ? TRUE : FALSE;
        const unsigned browser_slot = m_FileBrowser.GetSlotIndex();
        const char *slot_tag = cassette_browser
            ? locale->load_cassette_slot
            : (disk_browser
            ? ((browser_slot == 0u) ? locale->load_disk_drive1 : locale->load_disk_drive2)
            : ((browser_slot == 0u) ? locale->load_rom_slot1 : locale->load_rom_slot2));
        if (slot_tag == 0 || slot_tag[0] == '\0')
        {
            slot_tag = cassette_browser
                ? "TAPE"
                : (disk_browser
                ? ((browser_slot == 0u) ? "DRIVE1" : "DRIVE2")
                : ((browser_slot == 0u) ? "SLOT1" : "SLOT2"));
        }
        title = slot_tag;
        }
    }

    unsigned char_w = renderer->GetCharWidth();
    unsigned char_h = renderer->GetCharHeight();
    if (char_w == 0u) char_w = 8u;
    if (char_h == 0u) char_h = 16u;
    const unsigned cell_w = char_w * scale_x;
    const unsigned cell_h = char_h * scale_y;

    unsigned max_item_len = 0u;
    for (unsigned i = 0; i < item_count; ++i)
    {
        const unsigned len = ComboStrLen(items[i].label);
        if (len > max_item_len)
        {
            max_item_len = len;
        }
    }

    const boolean settings_without_title = (m_View == MenuViewSettings) ? TRUE : FALSE;
    const unsigned title_len = settings_without_title ? 0u : ComboStrLen(title);
    unsigned content_cols = max_item_len > title_len ? max_item_len : title_len;
    content_cols += 4u;
    if (content_cols < 18u)
    {
        content_cols = 18u;
    }
    if (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser)
    {
        /*
         * Browser width rule:
         * - use the largest entry name from current directory (already capped at 20 chars),
         * - keep width fixed for this directory while navigating inside it.
         */
        unsigned browser_name_cols = m_FileBrowser.GetMaxLabelLen();
        if (browser_name_cols > 20u)
        {
            browser_name_cols = 20u;
        }
        if (browser_name_cols < 5u)
        {
            browser_name_cols = 5u;
        }
        /* +3 for folder icon slot, +4 for inner paddings/borders, +1 safety col to avoid clipping. */
        const unsigned browser_content_cols = browser_name_cols + 8u;
        content_cols = browser_content_cols > title_len ? browser_content_cols : title_len;
    }
    const unsigned box_cols = content_cols;
    const boolean has_unload_hint = (m_View == MenuViewLoadRom || m_View == MenuViewLoadDisk || m_View == MenuViewLoadCassette) ? TRUE : FALSE;
    const unsigned box_rows = settings_without_title
        ? (item_count + 2u)
        : (item_count + 3u + (has_unload_hint ? 2u : 0u));
    const unsigned box_w_px = box_cols * cell_w;
    const unsigned box_h_px = box_rows * cell_h;

    const unsigned box_center_x = anchor_x + (anchor_w / 2u);
    const unsigned box_center_y = anchor_y + (anchor_h / 2u);
    unsigned box_x = box_center_x > (box_w_px / 2u) ? (box_center_x - (box_w_px / 2u)) : 0u;
    unsigned box_y = box_center_y > (box_h_px / 2u) ? (box_center_y - (box_h_px / 2u)) : 0u;

    if (box_x + box_w_px > screen_width)
    {
        box_x = screen_width > box_w_px ? (screen_width - box_w_px) : 0u;
    }
    if (box_y + box_h_px > screen_height)
    {
        box_y = screen_height > box_h_px ? (screen_height - box_h_px) : 0u;
    }

    if (m_ForceHighlightRedraw || m_ForceContentRedraw)
    {
        m_ForceRedraw = TRUE;
    }
    const boolean partial_row_redraw = FALSE;

    if (m_Visible
        && !m_ForceRedraw
     && !m_ForceHighlightRedraw
     && !m_ForceContentRedraw
     && m_Row == box_y
     && m_Col == box_x
     && m_LastBoxValid
     && m_LastBoxW == box_w_px
     && m_LastBoxH == box_h_px
     && m_LastDrawnSelected == m_Selected
     && m_LastDrawnView == m_View
     && m_LastDrawnScalePercent == m_ScalePercent
     && m_LastDrawnScaleMaxFitEnabled == m_ScaleMaxFitEnabled
     && m_LastDrawnMaxScalePercent == m_MaxScalePercent
     && m_LastDrawnLanguage == m_Language
     && m_LastDrawnScanlineMode == m_ScanlineMode
     && m_LastDrawnColorArtifactsEnabled == m_ColorArtifactsEnabled
     && m_LastDrawnGfx9000Enabled == m_Gfx9000Enabled
     && m_LastDrawnDiskRomEnabled == m_DiskRomEnabled
     && ComboTextEquals(m_LastDrawnCoreName, m_CoreName)
     && m_LastDrawnBootMode == m_BootMode
     && m_LastDrawnMachineProfile == m_MachineProfile
     && m_LastDrawnProcessorMode == m_ProcessorMode
     && m_LastDrawnRamMapperKb == m_RamMapperKb
     && m_LastDrawnMegaRamKb == m_MegaRamKb
     && m_LastDrawnFmMusic == m_FmMusicEnabled
     && m_LastDrawnSccCart == m_SccCartEnabled
     && m_LastDrawnSccDualCart == m_SccDualCartEnabled
     && m_LastDrawnSccDualCartAvailable == m_SccDualCartAvailable
     && m_LastDrawnAudioGainPercent == m_AudioGainPercent
     && ComboTextEquals(m_LastDrawnLoadRomSlot1Name, m_LoadRomSlot1Name)
     && ComboTextEquals(m_LastDrawnLoadRomSlot2Name, m_LoadRomSlot2Name)
     && ComboTextEquals(m_LastDrawnLoadDiskDrive1Name, m_LoadDiskDrive1Name)
     && ComboTextEquals(m_LastDrawnLoadDiskDrive2Name, m_LoadDiskDrive2Name)
     && ComboTextEquals(m_LastDrawnLoadCassetteName, m_LoadCassetteName)
     && (m_View != MenuViewLoadRomBrowser && m_View != MenuViewLoadDiskBrowser && m_View != MenuViewLoadCassetteBrowser
         ? TRUE
         : (m_LastDrawnBrowserTop == m_FileBrowser.GetTop()
            && m_LastDrawnBrowserCount == m_FileBrowser.GetCount()
            && m_LastDrawnBrowserSelectedAbs == m_FileBrowser.GetSelectedAbs()
            && m_LastDrawnBrowserMode == m_FileBrowser.GetMode()
            && m_LastDrawnBrowserSlot == m_FileBrowser.GetSlotIndex()
            && m_LastDrawnBrowserContentStamp == m_FileBrowser.GetContentStamp())))
    {
        return;
    }

    const unsigned title_x = box_x + ((box_cols - title_len) / 2u) * cell_w;
    const unsigned title_y = box_y;
    const boolean allow_backbuffer = TRUE;
    boolean use_backbuffer = FALSE;
    if (allow_backbuffer)
    {
        const unsigned backbuffer_w = anchor_w;
        const unsigned backbuffer_h = anchor_h;
        const unsigned backbuffer_need_pixels = backbuffer_w * backbuffer_h;
        if (backbuffer_need_pixels > 0u)
        {
            if (m_MenuBackbuffer == 0 || m_MenuBackbufferCapacityPixels < backbuffer_need_pixels)
            {
                TScreenColor *new_buffer = new TScreenColor[backbuffer_need_pixels];
                if (new_buffer != 0)
                {
                    if (m_MenuBackbuffer != 0)
                    {
                        delete[] m_MenuBackbuffer;
                    }
                    m_MenuBackbuffer = new_buffer;
                    m_MenuBackbufferCapacityPixels = backbuffer_need_pixels;
                }
            }
            use_backbuffer = (m_MenuBackbuffer != 0 && m_MenuBackbufferCapacityPixels >= backbuffer_need_pixels) ? TRUE : FALSE;
        }
    }
    if (use_backbuffer)
    {
        /* Full clear off-screen first; on-screen blit remains single-pass (no flick). */
        ComboClearPixels(m_MenuBackbuffer, anchor_w * anchor_h);

        TViewportScreenshotView snapshot;
        if (combo_viewport_screenshot_get(&snapshot))
        {
            const unsigned anchor_x1 = anchor_x + anchor_w;
            const unsigned anchor_y1 = anchor_y + anchor_h;
            const unsigned snap_x1 = snapshot.x + snapshot.w;
            const unsigned snap_y1 = snapshot.y + snapshot.h;
            const unsigned ix0 = (anchor_x > snapshot.x) ? anchor_x : snapshot.x;
            const unsigned iy0 = (anchor_y > snapshot.y) ? anchor_y : snapshot.y;
            const unsigned ix1 = (anchor_x1 < snap_x1) ? anchor_x1 : snap_x1;
            const unsigned iy1 = (anchor_y1 < snap_y1) ? anchor_y1 : snap_y1;
            if (ix1 > ix0 && iy1 > iy0)
            {
                const unsigned copy_w = ix1 - ix0;
                for (unsigned y = iy0; y < iy1; ++y)
                {
                    const unsigned src_off = (y - snapshot.y) * snapshot.stride + (ix0 - snapshot.x);
                    const unsigned dst_off = (y - anchor_y) * anchor_w + (ix0 - anchor_x);
                    const TScreenColor *src = snapshot.pixels + src_off;
                    TScreenColor *dst = m_MenuBackbuffer + dst_off;
                    for (unsigned x = 0u; x < copy_w; ++x)
                    {
                        dst[x] = src[x];
                    }
                }
            }
        }
        m_MenuBackbufferX = anchor_x;
        m_MenuBackbufferY = anchor_y;
        m_MenuBackbufferW = anchor_w;
        m_MenuBackbufferH = anchor_h;
        renderer->BeginOffscreen(anchor_x, anchor_y, anchor_w, anchor_h, m_MenuBackbuffer, anchor_w);
    }
    else
    {
        m_MenuBackbufferX = 0u;
        m_MenuBackbufferY = 0u;
        m_MenuBackbufferW = 0u;
        m_MenuBackbufferH = 0u;
    }
    renderer->FillRect(box_x, box_y, box_w_px, box_h_px, kPauseMenuBgColor);
    renderer->DrawRectBorder(box_x, box_y, box_w_px, box_h_px, kPauseMenuFgColor, 2u);
    if (!settings_without_title)
    {
        renderer->DrawText(title_x, title_y, title, title_style, scale_x, scale_y);
    }

    const boolean browser_with_scrollbar =
        ((m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser)
         && item_count > 0u
         && m_FileBrowser.GetCount() > item_count
         && box_cols > 8u);
    const unsigned bar_col = browser_with_scrollbar ? (box_cols - 3u) : 0u;

    const auto draw_row = [&](unsigned i)
    {
        const unsigned row = settings_without_title ? (1u + i) : (2u + i);
        const unsigned y = box_y + (row * cell_h);
        const unsigned text_len = ComboStrLen(items[i].label);
        const boolean settings_row = (m_View == MenuViewSettings || m_View == MenuViewJoystickMap);
        const boolean load_media_row = (m_View == MenuViewLoadRom || m_View == MenuViewLoadDisk || m_View == MenuViewLoadCassette);
        const boolean browser_row = (m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser);
        const boolean separator_row = (items[i].action == ComboMenuActionSeparator);
        const boolean selected_row = (items[i].selectable && i == m_Selected) ? TRUE : FALSE;

        if (selected_row && box_cols > 4u && !load_media_row)
        {
            unsigned highlight_cols = box_cols - 4u;
            if (browser_row && browser_with_scrollbar && bar_col > 4u)
            {
                const unsigned text_end_col = bar_col - 2u; /* keep one spacer col before bar */
                if (text_end_col > 2u)
                {
                    highlight_cols = text_end_col - 2u + 1u;
                }
            }
            renderer->FillRect(box_x + (2u * cell_w), y, highlight_cols * cell_w, cell_h, kPauseMenuFgColor);
        }

        if (separator_row)
        {
            const unsigned line_x = box_x + (2u * cell_w);
            const unsigned line_w = (box_cols > 4u) ? ((box_cols - 4u) * cell_w) : 0u;
            const unsigned line_y = y + (cell_h / 2u);
            if (line_w > 0u)
            {
                renderer->FillRect(line_x, line_y, line_w, 2u, kPauseMenuFgColor);
            }
            return;
        }

        if (settings_row || load_media_row)
        {
            const unsigned split = settings_row ? ComboFindFlagStart(items[i].label) : ComboFindTabSplit(items[i].label);
            const unsigned left_len = split < text_len ? split : text_len;
            const unsigned flag_start = load_media_row && split < text_len ? (split + 1u) : split;
            const unsigned flag_len = split < text_len ? (text_len - flag_start) : 0u;
            const unsigned inner_left_col = 2u;
            const unsigned inner_right_col = box_cols > 2u ? (box_cols - 3u) : 0u;
            const unsigned x_left = box_x + (inner_left_col * cell_w);
            const unsigned x_flag = flag_len > 0u
                ? box_x + ((inner_right_col + 1u - flag_len) * cell_w)
                : 0u;
            const TGfxTextStyle left_style = (settings_row && selected_row) ? selected_style : normal_style;
            TGfxTextStyle flag_style = left_style;

            if (load_media_row && box_cols > 4u)
            {
                /* Keep right-aligned value area visually stable by clearing row span first. */
                renderer->FillRect(box_x + (2u * cell_w), y, (box_cols - 4u) * cell_w, cell_h, kPauseMenuBgColor);
            }

            if (load_media_row && selected_row)
            {
                unsigned x_sel = x_left + (left_len * cell_w);
                const unsigned x_end = box_x + ((inner_right_col + 1u) * cell_w);
                if (x_sel < x_end)
                {
                    renderer->FillRect(x_sel, y, x_end - x_sel, cell_h, kPauseMenuFgColor);
                }
                flag_style = selected_style; /* draw value in inverted style over white bar */
            }
            if (flag_len > 0u && x_flag > x_left + cell_w)
            {
                char left_buf[48];
                char flag_buf[32];
                ComboSliceCopy(left_buf, sizeof(left_buf), items[i].label, 0u, left_len);
                ComboSliceCopy(flag_buf, sizeof(flag_buf), items[i].label, flag_start, flag_len);
                renderer->DrawText(x_left, y, left_buf, left_style, scale_x, scale_y);
                renderer->DrawText(x_flag, y, flag_buf, flag_style, scale_x, scale_y);
            }
            else
            {
                const unsigned x = box_x + ((box_cols - text_len) / 2u) * cell_w;
                renderer->DrawText(x, y, items[i].label, left_style, scale_x, scale_y);
            }
        }
        else if (browser_row)
        {
            const unsigned x = box_x + (2u * cell_w);
            const TGfxTextStyle style = (items[i].selectable && i == m_Selected) ? selected_style : normal_style;
            const boolean draw_folder_icon = (m_FileBrowser.IsDirVisible(i) && !m_FileBrowser.IsParentVisible(i)) ? TRUE : FALSE;
            unsigned max_label_chars = 20u;
            if (browser_with_scrollbar && bar_col > 4u)
            {
                /* Keep one blank column between filename and scrollbar. */
                const unsigned text_end_col = bar_col - 2u;
                const unsigned dyn_max = (text_end_col > 2u) ? (text_end_col - 2u + 1u) : 0u;
                max_label_chars = (dyn_max < max_label_chars) ? dyn_max : max_label_chars;
            }
            if (draw_folder_icon && max_label_chars > 3u)
            {
                max_label_chars -= 3u;
            }
            char clipped_label[40];
            ComboSliceCopy(clipped_label, sizeof(clipped_label), items[i].label, 0u, max_label_chars);
            if (draw_folder_icon)
            {
                renderer->DrawText(x, y, "\x1E\x1F", style, scale_x, scale_y);
                renderer->DrawText(x + (3u * cell_w), y, clipped_label, style, scale_x, scale_y);
            }
            else
            {
                renderer->DrawText(x, y, clipped_label, style, scale_x, scale_y);
            }
        }
        else
        {
            const unsigned x = box_x + ((box_cols - text_len) / 2u) * cell_w;
            const TGfxTextStyle style = (items[i].selectable && i == m_Selected) ? selected_style : normal_style;
            renderer->DrawText(x, y, items[i].label, style, scale_x, scale_y);
        }
    };

    for (unsigned i = 0; i < item_count; ++i)
    {
        draw_row(i);
    }

    if (browser_with_scrollbar)
    {
        const unsigned bar_x = box_x + (bar_col * cell_w);
        const unsigned top_row = 2u;
        const unsigned bottom_row = 2u + item_count - 1u;
        const unsigned arrow_up_row = (top_row > 0u) ? (top_row - 1u) : top_row;
        const unsigned arrow_down_row = bottom_row + 1u;
        const unsigned top_y = box_y + (arrow_up_row * cell_h);
        const unsigned bottom_y = box_y + (arrow_down_row * cell_h);

        if (bottom_row > top_row + 1u)
        {
            /* Keep one full row gap from arrows to avoid overlap with arrow glyphs. */
            const unsigned track_start_row = top_row + 1u;
            const unsigned track_end_row = (bottom_row > 0u) ? (bottom_row - 1u) : bottom_row;
            if (track_end_row >= track_start_row)
            {
                const unsigned track_len = track_end_row - track_start_row + 1u;
                const unsigned max_top = m_FileBrowser.GetCount() - item_count;
                unsigned thumb_row = track_start_row;
                if (max_top > 0u)
                {
                    const unsigned rel = ((track_len - 1u) * m_FileBrowser.GetTop()) / max_top;
                    thumb_row = track_start_row + rel;
                }
                const unsigned thumb_y = box_y + (thumb_row * cell_h);
                renderer->DrawText(bar_x, thumb_y, "\x1B", normal_style, scale_x, scale_y);
            }
        }
        renderer->DrawText(bar_x, top_y, "\x1C", normal_style, scale_x, scale_y);
        renderer->DrawText(bar_x, bottom_y, "\x1D", normal_style, scale_x, scale_y);
    }

    if (has_unload_hint)
    {
        const char *hint = (m_View == MenuViewLoadDisk)
            ? locale->load_disk_unload_hint
            : ((m_View == MenuViewLoadCassette) ? locale->load_cassette_unload_hint : locale->load_rom_unload_hint);
        if (hint == 0 || hint[0] == '\0')
        {
            hint = (m_View == MenuViewLoadDisk)
                ? "[DEL] Eject Disk"
                : ((m_View == MenuViewLoadCassette) ? "[DEL] Eject Tape" : "[DEL] Unload ROM");
        }
        const unsigned hint_len = ComboStrLen(hint);
        const unsigned hint_row = item_count + 3u; /* one blank line above */
        const unsigned hint_y = box_y + (hint_row * cell_h);
        const unsigned hint_x = box_x + ((box_cols - hint_len) / 2u) * cell_w;
        renderer->DrawText(hint_x, hint_y, hint, normal_style, scale_x, scale_y);
    }
    m_Row = box_y;
    m_Col = box_x;
    m_LastDrawnSelected = m_Selected;
    m_LastDrawnView = m_View;
    m_LastDrawnScalePercent = m_ScalePercent;
    m_LastDrawnScaleMaxFitEnabled = m_ScaleMaxFitEnabled;
    m_LastDrawnMaxScalePercent = m_MaxScalePercent;
    m_LastDrawnLanguage = m_Language;
    m_LastDrawnScanlineMode = m_ScanlineMode;
    m_LastDrawnColorArtifactsEnabled = m_ColorArtifactsEnabled;
    m_LastDrawnGfx9000Enabled = m_Gfx9000Enabled;
    m_LastDrawnDiskRomEnabled = m_DiskRomEnabled;
    CopySmallText(m_LastDrawnCoreName, sizeof(m_LastDrawnCoreName), m_CoreName);
    m_LastDrawnBootMode = m_BootMode;
    m_LastDrawnMachineProfile = m_MachineProfile;
    m_LastDrawnProcessorMode = m_ProcessorMode;
    m_LastDrawnRamMapperKb = m_RamMapperKb;
    m_LastDrawnMegaRamKb = m_MegaRamKb;
    m_LastDrawnFmMusic = m_FmMusicEnabled;
    m_LastDrawnSccCart = m_SccCartEnabled;
    m_LastDrawnSccDualCart = m_SccDualCartEnabled;
    m_LastDrawnSccDualCartAvailable = m_SccDualCartAvailable;
    m_LastDrawnAudioGainPercent = m_AudioGainPercent;
    CopySmallText(m_LastDrawnLoadRomSlot1Name, sizeof(m_LastDrawnLoadRomSlot1Name), m_LoadRomSlot1Name);
    CopySmallText(m_LastDrawnLoadRomSlot2Name, sizeof(m_LastDrawnLoadRomSlot2Name), m_LoadRomSlot2Name);
    CopySmallText(m_LastDrawnLoadDiskDrive1Name, sizeof(m_LastDrawnLoadDiskDrive1Name), m_LoadDiskDrive1Name);
    CopySmallText(m_LastDrawnLoadDiskDrive2Name, sizeof(m_LastDrawnLoadDiskDrive2Name), m_LoadDiskDrive2Name);
    m_LastDrawnBrowserTop = m_FileBrowser.GetTop();
    m_LastDrawnBrowserCount = m_FileBrowser.GetCount();
    m_LastDrawnBrowserSelectedAbs = m_FileBrowser.GetSelectedAbs();
    m_LastDrawnBrowserMode = m_FileBrowser.GetMode();
    m_LastDrawnBrowserSlot = m_FileBrowser.GetSlotIndex();
    m_LastDrawnBrowserContentStamp = m_FileBrowser.GetContentStamp();
    m_LastBoxX = box_x;
    m_LastBoxY = box_y;
    m_LastBoxW = box_w_px;
    m_LastBoxH = box_h_px;
    m_LastBoxValid = TRUE;
    if (use_backbuffer)
    {
        renderer->EndOffscreenBlit();
    }
    m_ForceRedraw = FALSE;
    m_ForceHighlightRedraw = FALSE;
    m_ForceContentRedraw = FALSE;
    if ((m_View == MenuViewLoadRomBrowser || m_View == MenuViewLoadDiskBrowser || m_View == MenuViewLoadCassetteBrowser) && m_BrowserWarmupRedraws > 0u)
    {
        --m_BrowserWarmupRedraws;
        m_ForceRedraw = TRUE;
    }
    m_Visible = TRUE;
}
