#include "combo_file_browser.h"

extern "C" {
#include <fatfs/ff.h>
}

static const char *kRomsRootPath = "SD:/sms/roms";
static const char *kDisksRootPath = "SD:/sms/roms";
static const char *kCassetteRootPath = "SD:/sms/roms";
static const unsigned kRepeatDelayFrames = 12u;  /* ~200ms em 60Hz */
static const unsigned kRepeatPeriodFrames = 2u;  /* ~30 passos/s em 60Hz */

static char ComboToUpperAscii(char ch)
{
    if (ch >= 'a' && ch <= 'z')
    {
        return (char) (ch - 'a' + 'A');
    }
    return ch;
}

static int ComboIsFileChar83(char ch)
{
    if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
    {
        return 1;
    }
    return (ch == '_' || ch == '$' || ch == '~' || ch == '!' || ch == '#' || ch == '%' || ch == '-');
}

static int ComboPathEqualsIgnoreCase(const char *a, const char *b)
{
    if (a == 0 || b == 0)
    {
        return 0;
    }
    unsigned i = 0u;
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (ComboToUpperAscii(a[i]) != ComboToUpperAscii(b[i]))
        {
            return 0;
        }
        ++i;
    }
    return (a[i] == '\0' && b[i] == '\0') ? 1 : 0;
}

static int ComboCompareIgnoreCase(const char *a, const char *b)
{
    if (a == 0 && b == 0) return 0;
    if (a == 0) return -1;
    if (b == 0) return 1;
    unsigned i = 0u;
    while (a[i] != '\0' && b[i] != '\0')
    {
        const char ca = ComboToUpperAscii(a[i]);
        const char cb = ComboToUpperAscii(b[i]);
        if (ca < cb) return -1;
        if (ca > cb) return 1;
        ++i;
    }
    if (a[i] == '\0' && b[i] == '\0') return 0;
    return (a[i] == '\0') ? -1 : 1;
}

static unsigned ComboStrLen(const char *text)
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

static int ComboEndsWithIgnoreCase(const char *text, const char *suffix)
{
    const unsigned text_len = ComboStrLen(text);
    const unsigned suffix_len = ComboStrLen(suffix);
    if (suffix_len == 0u || text_len < suffix_len)
    {
        return 0;
    }
    const unsigned start = text_len - suffix_len;
    for (unsigned i = 0u; i < suffix_len; ++i)
    {
        if (ComboToUpperAscii(text[start + i]) != ComboToUpperAscii(suffix[i]))
        {
            return 0;
        }
    }
    return 1;
}

static int ComboIsSupportedFileForMode(unsigned mode, const char *name)
{
    if (name == 0 || name[0] == '\0' || name[0] == '.')
    {
        return 0;
    }
    if (mode == (unsigned) ComboFileBrowserModeDisk)
    {
        return ComboEndsWithIgnoreCase(name, ".dsk")
            || ComboEndsWithIgnoreCase(name, ".img")
            || ComboEndsWithIgnoreCase(name, ".di1")
            || ComboEndsWithIgnoreCase(name, ".di2");
    }
    if (mode == (unsigned) ComboFileBrowserModeCassette)
    {
        return ComboEndsWithIgnoreCase(name, ".cas");
    }
    return ComboEndsWithIgnoreCase(name, ".sms");
}

static int ComboJoinPath(char *dst, unsigned dst_size, const char *base, const char *name)
{
    if (dst == 0 || dst_size == 0u || base == 0 || name == 0)
    {
        return 0;
    }
    const unsigned base_len = ComboStrLen(base);
    const unsigned name_len = ComboStrLen(name);
    const unsigned needs_sep = (base_len > 0u && base[base_len - 1u] != '/') ? 1u : 0u;
    const unsigned total = base_len + needs_sep + name_len + 1u;
    if (total > dst_size)
    {
        return 0;
    }
    unsigned cursor = 0u;
    for (unsigned i = 0u; i < base_len; ++i)
    {
        dst[cursor++] = base[i];
    }
    if (needs_sep)
    {
        dst[cursor++] = '/';
    }
    for (unsigned i = 0u; i < name_len; ++i)
    {
        dst[cursor++] = name[i];
    }
    dst[cursor] = '\0';
    return 1;
}

static void ComboCopyParentDir(char *out_dir, unsigned out_size, const char *path)
{
    if (out_dir == 0 || out_size == 0u)
    {
        return;
    }
    out_dir[0] = '\0';
    if (path == 0 || path[0] == '\0')
    {
        return;
    }

    unsigned len = 0u;
    while (path[len] != '\0')
    {
        ++len;
    }

    if (len == 0u)
    {
        return;
    }

    unsigned slash = len;
    while (slash > 0u && path[slash - 1u] != '/' && path[slash - 1u] != '\\')
    {
        --slash;
    }

    if (slash == 0u)
    {
        return;
    }

    unsigned copy_len = slash - 1u;
    if (copy_len >= out_size)
    {
        copy_len = out_size - 1u;
    }
    for (unsigned i = 0u; i < copy_len; ++i)
    {
        out_dir[i] = path[i];
    }
    out_dir[copy_len] = '\0';
}

static const char *ComboBaseNameFromPath(const char *path)
{
    if (path == 0)
    {
        return 0;
    }
    const char *base = path;
    for (const char *p = path; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\')
        {
            base = p + 1;
        }
    }
    return base;
}

CComboFileBrowser::CComboFileBrowser(void)
: m_Mode((unsigned) ComboFileBrowserModeRom),
  m_SlotIndex(0u),
  m_Count(0u),
  m_SelectedAbs(0u),
  m_Top(0u),
  m_SourceIndex{0},
  m_Names{{0}},
  m_Paths{{0}},
  m_IsDir{FALSE},
  m_IsParent{FALSE},
  m_CurrentDir{0},
  m_LastUpPressed(FALSE),
  m_LastDownPressed(FALSE),
  m_LastPageUpPressed(FALSE),
  m_LastPageDownPressed(FALSE),
  m_UpHoldFrames(0u),
  m_DownHoldFrames(0u),
  m_ContentStamp(1u)
{
    CopySmallText(m_CurrentDir, sizeof(m_CurrentDir), kRomsRootPath);
}

void CComboFileBrowser::Reset(void)
{
    m_Mode = (unsigned) ComboFileBrowserModeRom;
    m_SlotIndex = 0u;
    m_Count = 0u;
    m_SelectedAbs = 0u;
    m_Top = 0u;
    CopySmallText(m_CurrentDir, sizeof(m_CurrentDir), kRomsRootPath);
    m_LastUpPressed = FALSE;
    m_LastDownPressed = FALSE;
    m_LastPageUpPressed = FALSE;
    m_LastPageDownPressed = FALSE;
    m_UpHoldFrames = 0u;
    m_DownHoldFrames = 0u;
    ++m_ContentStamp;
    for (unsigned i = 0u; i < kMaxEntries; ++i)
    {
        m_SourceIndex[i] = 0u;
        m_IsDir[i] = FALSE;
        m_IsParent[i] = FALSE;
        m_Names[i][0] = '\0';
        m_Paths[i][0] = '\0';
    }
}

void CComboFileBrowser::Open(unsigned mode, unsigned slot_index)
{
    m_Mode = (mode == (unsigned) ComboFileBrowserModeDisk || mode == (unsigned) ComboFileBrowserModeCassette)
        ? mode
        : (unsigned) ComboFileBrowserModeRom;
    m_SlotIndex = slot_index > 1u ? 1u : slot_index;
    CopySmallText(m_CurrentDir, sizeof(m_CurrentDir), RootPath());
    m_LastUpPressed = FALSE;
    m_LastDownPressed = FALSE;
    m_LastPageUpPressed = FALSE;
    m_LastPageDownPressed = FALSE;
    m_UpHoldFrames = 0u;
    m_DownHoldFrames = 0u;
    RebuildCatalog();
}

void CComboFileBrowser::EnterDirectory(const char *dir_path)
{
    if (dir_path == 0 || dir_path[0] == '\0')
    {
        return;
    }
    if (!IsPathInsideRoot(dir_path, RootPath()))
    {
        return;
    }

    CopySmallText(m_CurrentDir, sizeof(m_CurrentDir), dir_path);
    RebuildCatalog();
}

unsigned CComboFileBrowser::GetMode(void) const
{
    return m_Mode;
}

unsigned CComboFileBrowser::GetSlotIndex(void) const
{
    return m_SlotIndex;
}

unsigned CComboFileBrowser::GetCount(void) const
{
    return m_Count;
}

unsigned CComboFileBrowser::GetTop(void) const
{
    return m_Top;
}

unsigned CComboFileBrowser::GetSelectedAbs(void) const
{
    return m_SelectedAbs;
}

unsigned CComboFileBrowser::GetSelectedSourceIndex(void) const
{
    if (m_SelectedAbs >= m_Count)
    {
        return 0u;
    }
    return m_SourceIndex[m_SelectedAbs];
}

unsigned CComboFileBrowser::GetSelectedVisibleIndex(void) const
{
    if (m_Count == 0u)
    {
        return 0u;
    }
    if (m_SelectedAbs < m_Top)
    {
        return 0u;
    }
    const unsigned max_visible = GetVisibleCount();
    unsigned visible = m_SelectedAbs - m_Top;
    if (max_visible == 0u)
    {
        return 0u;
    }
    if (visible >= max_visible)
    {
        visible = max_visible - 1u;
    }
    return visible;
}

unsigned CComboFileBrowser::GetSourceIndexVisible(unsigned visible_index) const
{
    const unsigned abs = m_Top + visible_index;
    if (abs >= m_Count)
    {
        return 0u;
    }
    return m_SourceIndex[abs];
}

unsigned CComboFileBrowser::GetVisibleCount(void) const
{
    if (m_Count <= m_Top)
    {
        return 0u;
    }
    const unsigned remaining = m_Count - m_Top;
    return remaining < kVisibleRows ? remaining : kVisibleRows;
}

unsigned CComboFileBrowser::GetMaxLabelLen(void) const
{
    unsigned max_len = 0u;
    for (unsigned i = 0u; i < m_Count; ++i)
    {
        unsigned len = 0u;
        while (m_Names[i][len] != '\0')
        {
            ++len;
        }
        if (len > max_len)
        {
            max_len = len;
        }
    }
    return max_len;
}

unsigned CComboFileBrowser::GetContentStamp(void) const
{
    return m_ContentStamp;
}

const char *CComboFileBrowser::GetLabelVisible(unsigned visible_index) const
{
    const unsigned abs = m_Top + visible_index;
    if (abs >= m_Count)
    {
        return "";
    }
    return m_Names[abs];
}

boolean CComboFileBrowser::IsDirVisible(unsigned visible_index) const
{
    const unsigned abs = m_Top + visible_index;
    if (abs >= m_Count)
    {
        return FALSE;
    }
    return m_IsDir[abs];
}

boolean CComboFileBrowser::IsParentVisible(unsigned visible_index) const
{
    const unsigned abs = m_Top + visible_index;
    if (abs >= m_Count)
    {
        return FALSE;
    }
    return m_IsParent[abs];
}

TComboFileBrowserEvent CComboFileBrowser::ProcessInput(boolean up_pressed,
                                                       boolean down_pressed,
                                                       boolean page_up_pressed,
                                                       boolean page_down_pressed,
                                                       boolean enter_edge,
                                                       char *out_path,
                                                       unsigned out_path_size)
{
    if (out_path != 0 && out_path_size > 0u)
    {
        out_path[0] = '\0';
    }

    if (m_Count == 0u)
    {
        m_LastUpPressed = up_pressed;
        m_LastDownPressed = down_pressed;
        m_LastPageUpPressed = page_up_pressed;
        m_LastPageDownPressed = page_down_pressed;
        m_UpHoldFrames = 0u;
        m_DownHoldFrames = 0u;
        return ComboFileBrowserEventNone;
    }

    const unsigned visible_window = (m_Count < kVisibleRows) ? m_Count : kVisibleRows;

    boolean up_step = FALSE;
    boolean down_step = FALSE;
    if (up_pressed)
    {
        if (!m_LastUpPressed)
        {
            up_step = TRUE;
            m_UpHoldFrames = 0u;
        }
        else
        {
            ++m_UpHoldFrames;
            if (m_UpHoldFrames >= kRepeatDelayFrames)
            {
                const unsigned tick = m_UpHoldFrames - kRepeatDelayFrames;
                if ((tick % kRepeatPeriodFrames) == 0u)
                {
                    up_step = TRUE;
                }
            }
        }
    }
    else
    {
        m_UpHoldFrames = 0u;
    }

    if (down_pressed)
    {
        if (!m_LastDownPressed)
        {
            down_step = TRUE;
            m_DownHoldFrames = 0u;
        }
        else
        {
            ++m_DownHoldFrames;
            if (m_DownHoldFrames >= kRepeatDelayFrames)
            {
                const unsigned tick = m_DownHoldFrames - kRepeatDelayFrames;
                if ((tick % kRepeatPeriodFrames) == 0u)
                {
                    down_step = TRUE;
                }
            }
        }
    }
    else
    {
        m_DownHoldFrames = 0u;
    }

    if (up_step)
    {
        m_SelectedAbs = (m_SelectedAbs == 0u) ? (m_Count - 1u) : (m_SelectedAbs - 1u);
    }
    if (down_step)
    {
        m_SelectedAbs = (m_SelectedAbs + 1u) % m_Count;
    }

    const boolean page_up_edge = (page_up_pressed && !m_LastPageUpPressed) ? TRUE : FALSE;
    const boolean page_down_edge = (page_down_pressed && !m_LastPageDownPressed) ? TRUE : FALSE;
    if (page_up_edge)
    {
        if (m_SelectedAbs > visible_window)
        {
            m_SelectedAbs -= visible_window;
        }
        else
        {
            m_SelectedAbs = 0u;
        }
    }
    if (page_down_edge)
    {
        const unsigned max_index = m_Count - 1u;
        if (m_SelectedAbs + visible_window < m_Count)
        {
            m_SelectedAbs += visible_window;
        }
        else
        {
            m_SelectedAbs = max_index;
        }
    }

    /* Keep selection and visible window strictly synchronized (no one-step lag). */
    unsigned max_top = 0u;
    if (m_Count > visible_window)
    {
        max_top = m_Count - visible_window;
    }
    if (m_Top > max_top)
    {
        m_Top = max_top;
    }
    if (m_SelectedAbs < m_Top)
    {
        m_Top = m_SelectedAbs;
    }
    if (m_SelectedAbs >= m_Top + visible_window)
    {
        m_Top = m_SelectedAbs + 1u - visible_window;
    }
    if (m_Top > max_top)
    {
        m_Top = max_top;
    }

    m_LastUpPressed = up_pressed;
    m_LastDownPressed = down_pressed;
    m_LastPageUpPressed = page_up_pressed;
    m_LastPageDownPressed = page_down_pressed;

    if (enter_edge)
    {
        if (m_IsDir[m_SelectedAbs])
        {
            if (out_path != 0 && out_path_size > 0u)
            {
                CopySmallText(out_path, out_path_size, m_Paths[m_SelectedAbs]);
            }
            return ComboFileBrowserEventEnteredDirectory;
        }
        if (out_path != 0 && out_path_size > 0u)
        {
            CopySmallText(out_path, out_path_size, m_Paths[m_SelectedAbs]);
        }
        return ComboFileBrowserEventSelectedFile;
    }

    return (up_step || down_step || page_up_edge || page_down_edge)
        ? ComboFileBrowserEventSelectionChanged
        : ComboFileBrowserEventNone;
}

boolean CComboFileBrowser::JumpToFirstContaining(char letter)
{
    if (m_Count == 0u)
    {
        return FALSE;
    }

    char target = ComboToUpperAscii(letter);
    if (target < 'A' || target > 'Z')
    {
        return FALSE;
    }

    unsigned found = m_Count;
    unsigned found_contains = m_Count;
    for (unsigned i = 0u; i < m_Count; ++i)
    {
        const char *name = m_Names[i];
        if (name == 0)
        {
            continue;
        }
        for (unsigned j = 0u; name[j] != '\0'; ++j)
        {
            const char ch = ComboToUpperAscii(name[j]);
            if (j == 0u && ch == target)
            {
                found = i;
                break;
            }
            if (found_contains == m_Count && ch == target)
            {
                found_contains = i;
            }
        }
        if (found != m_Count)
        {
            break;
        }
    }

    if (found == m_Count)
    {
        found = found_contains;
    }
    if (found == m_Count)
    {
        return FALSE;
    }

    const unsigned visible_window = (m_Count < kVisibleRows) ? m_Count : kVisibleRows;
    m_SelectedAbs = found;
    if (m_SelectedAbs < m_Top)
    {
        m_Top = m_SelectedAbs;
    }
    if (visible_window > 0u && m_SelectedAbs >= m_Top + visible_window)
    {
        m_Top = m_SelectedAbs + 1u - visible_window;
    }
    if (m_Count > visible_window)
    {
        const unsigned max_top = m_Count - visible_window;
        if (m_Top > max_top)
        {
            m_Top = max_top;
        }
    }
    else
    {
        m_Top = 0u;
    }
    return TRUE;
}

void CComboFileBrowser::CopySmallText(char *dst, unsigned dst_size, const char *src)
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

void CComboFileBrowser::FormatName20NoExt(const char *path, char out_name[21])
{
    if (out_name == 0)
    {
        return;
    }

    out_name[0] = '\0';
    if (path == 0 || path[0] == '\0')
    {
        return;
    }

    const char *base = path;
    for (const char *p = path; *p != '\0'; ++p)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
        {
            base = p + 1;
        }
    }
    if (base[0] == '\0')
    {
        return;
    }

    const char *dot = 0;
    for (const char *p = base; *p != '\0'; ++p)
    {
        if (*p == '.')
        {
            dot = p;
        }
    }

    unsigned n = 0u;
    for (const char *p = base; *p != '\0' && p != dot && n < 20u; ++p)
    {
        char ch = ComboToUpperAscii(*p);
        if (ComboIsFileChar83(ch))
        {
            out_name[n++] = ch;
        }
    }
    if (n == 0u)
    {
        out_name[n++] = '_';
    }
    out_name[n] = '\0';
}

void CComboFileBrowser::FormatTitle20(const char *title, char out_name[21])
{
    unsigned n = 0u;
    int last_space = 1;

    if (out_name == 0)
    {
        return;
    }

    out_name[0] = '\0';
    if (title == 0 || title[0] == '\0')
    {
        return;
    }

    for (unsigned i = 0u; title[i] != '\0' && n < 20u; ++i)
    {
        char ch = title[i];
        if (ch == '\t' || ch == '\r' || ch == '\n')
        {
            ch = ' ';
        }
        if ((unsigned char) ch < 32u || (unsigned char) ch > 126u)
        {
            continue;
        }
        if (ch == ' ')
        {
            if (last_space)
            {
                continue;
            }
            last_space = 1;
        }
        else
        {
            last_space = 0;
        }
        out_name[n++] = ch;
    }

    while (n > 0u && out_name[n - 1u] == ' ')
    {
        --n;
    }
    if (n == 0u)
    {
        out_name[n++] = '_';
    }
    out_name[n] = '\0';
}

boolean CComboFileBrowser::IsPathInsideRoot(const char *path, const char *root)
{
    if (path == 0 || root == 0 || root[0] == '\0')
    {
        return FALSE;
    }

    unsigned i = 0u;
    for (; root[i] != '\0'; ++i)
    {
        if (path[i] == '\0')
        {
            return FALSE;
        }
        if (ComboToUpperAscii(path[i]) != ComboToUpperAscii(root[i]))
        {
            return FALSE;
        }
    }
    const char tail = path[i];
    return (tail == '\0' || tail == '/' || tail == '\\') ? TRUE : FALSE;
}

void CComboFileBrowser::InsertSorted(unsigned section_start,
                                     const char *name,
                                     const char *path,
                                     boolean is_dir,
                                     boolean is_parent,
                                     unsigned source_index)
{
    if (name == 0 || path == 0 || m_Count >= kMaxEntries)
    {
        return;
    }

    unsigned insert_at = m_Count;
    for (unsigned pos = section_start; pos < m_Count; ++pos)
    {
        if (ComboCompareIgnoreCase(name, m_Names[pos]) < 0)
        {
            insert_at = pos;
            break;
        }
    }

    if (insert_at < m_Count)
    {
        for (unsigned move = m_Count; move > insert_at; --move)
        {
            m_SourceIndex[move] = m_SourceIndex[move - 1u];
            CopySmallText(m_Names[move], sizeof(m_Names[0]), m_Names[move - 1u]);
            CopySmallText(m_Paths[move], sizeof(m_Paths[0]), m_Paths[move - 1u]);
            m_IsDir[move] = m_IsDir[move - 1u];
            m_IsParent[move] = m_IsParent[move - 1u];
        }
    }

    m_SourceIndex[insert_at] = source_index;
    CopySmallText(m_Names[insert_at], sizeof(m_Names[0]), name);
    CopySmallText(m_Paths[insert_at], sizeof(m_Paths[0]), path);
    m_IsDir[insert_at] = is_dir;
    m_IsParent[insert_at] = is_parent;
    ++m_Count;
}

void CComboFileBrowser::RebuildCatalog(void)
{
    const char *root_path = RootPath();
    m_Count = 0u;
    m_SelectedAbs = 0u;
    m_Top = 0u;

    if (!IsPathInsideRoot(m_CurrentDir, root_path))
    {
        CopySmallText(m_CurrentDir, sizeof(m_CurrentDir), root_path);
    }

    if (!ComboPathEqualsIgnoreCase(m_CurrentDir, root_path) && m_Count < kMaxEntries)
    {
        char parent_dir[192];
        ComboCopyParentDir(parent_dir, sizeof(parent_dir), m_CurrentDir);
        if (!IsPathInsideRoot(parent_dir, root_path) || parent_dir[0] == '\0')
        {
            CopySmallText(parent_dir, sizeof(parent_dir), root_path);
        }
        CopySmallText(m_Names[m_Count], sizeof(m_Names[0]), "..");
        CopySmallText(m_Paths[m_Count], sizeof(m_Paths[0]), parent_dir);
        m_SourceIndex[m_Count] = 0u;
        m_IsDir[m_Count] = TRUE;
        m_IsParent[m_Count] = TRUE;
        ++m_Count;
    }

    for (unsigned pass = 0u; pass < 2u && m_Count < kMaxEntries; ++pass)
    {
        const unsigned pass_start = m_Count;
        DIR dir;
        FILINFO fno;
        if (f_opendir(&dir, m_CurrentDir) != FR_OK)
        {
            break;
        }

        for (;;)
        {
            if (m_Count >= kMaxEntries)
            {
                break;
            }
            fno.fname[0] = '\0';
            const FRESULT fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == '\0')
            {
                break;
            }
            if (fno.fname[0] == '.')
            {
                continue;
            }

            const boolean is_dir = ((fno.fattrib & AM_DIR) != 0u) ? TRUE : FALSE;
            if ((pass == 0u && !is_dir) || (pass == 1u && is_dir))
            {
                continue;
            }
            if (!is_dir && !ComboIsSupportedFileForMode(m_Mode, fno.fname))
            {
                continue;
            }

            char full_path[192];
            if (!ComboJoinPath(full_path, sizeof(full_path), m_CurrentDir, fno.fname))
            {
                continue;
            }

            char new_name[21];
            FormatName20NoExt(full_path, new_name);
            InsertSorted(pass_start, new_name, full_path, is_dir, FALSE, m_Count);
        }

        f_closedir(&dir);
    }
    ++m_ContentStamp;
}

const char *CComboFileBrowser::RootPath(void) const
{
    if (m_Mode == (unsigned) ComboFileBrowserModeDisk)
    {
        return kDisksRootPath;
    }
    if (m_Mode == (unsigned) ComboFileBrowserModeCassette)
    {
        return kCassetteRootPath;
    }
    return kRomsRootPath;
}
