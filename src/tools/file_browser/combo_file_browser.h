#ifndef SMSBARE_CIRCLE_SMSBARE_FILE_BROWSER_H
#define SMSBARE_CIRCLE_SMSBARE_FILE_BROWSER_H

#include <circle/types.h>

enum TComboFileBrowserMode {
    ComboFileBrowserModeRom = 0,
    ComboFileBrowserModeDisk = 1,
    ComboFileBrowserModeCassette = 2
};

enum TComboFileBrowserEvent {
    ComboFileBrowserEventNone = 0,
    ComboFileBrowserEventSelectionChanged = 1,
    ComboFileBrowserEventEnteredDirectory = 2,
    ComboFileBrowserEventSelectedFile = 3
};

class CComboFileBrowser
{
public:
    static const unsigned kMaxEntries = 1024u;
    static const unsigned kVisibleRows = 10u;

    CComboFileBrowser(void);

    void Reset(void);
    void Open(unsigned mode, unsigned slot_index);
    void EnterDirectory(const char *dir_path);
    unsigned GetMode(void) const;
    unsigned GetSlotIndex(void) const;
    unsigned GetCount(void) const;
    unsigned GetTop(void) const;
    unsigned GetSelectedAbs(void) const;
    unsigned GetSelectedSourceIndex(void) const;
    unsigned GetSelectedVisibleIndex(void) const;
    unsigned GetSourceIndexVisible(unsigned visible_index) const;
    unsigned GetVisibleCount(void) const;
    unsigned GetMaxLabelLen(void) const;
    unsigned GetContentStamp(void) const;
    const char *GetLabelVisible(unsigned visible_index) const;
    boolean IsDirVisible(unsigned visible_index) const;
    boolean IsParentVisible(unsigned visible_index) const;
    TComboFileBrowserEvent ProcessInput(boolean up_pressed,
                                        boolean down_pressed,
                                        boolean page_up_pressed,
                                        boolean page_down_pressed,
                                        boolean enter_edge,
                                        char *out_path,
                                        unsigned out_path_size);
    boolean JumpToFirstContaining(char letter);

private:
    static void CopySmallText(char *dst, unsigned dst_size, const char *src);
    static void FormatName20NoExt(const char *path, char out_name[21]);
    static void FormatTitle20(const char *title, char out_name[21]);
    static boolean IsPathInsideRoot(const char *path, const char *root);
    void InsertSorted(unsigned section_start,
                      const char *name,
                      const char *path,
                      boolean is_dir,
                      boolean is_parent,
                      unsigned source_index);
    void RebuildCatalog(void);
    const char *RootPath(void) const;

private:
    unsigned m_Mode;
    unsigned m_SlotIndex;
    unsigned m_Count;
    unsigned m_SelectedAbs;
    unsigned m_Top;
    unsigned m_SourceIndex[kMaxEntries];
    char m_Names[kMaxEntries][21];
    char m_Paths[kMaxEntries][192];
    boolean m_IsDir[kMaxEntries];
    boolean m_IsParent[kMaxEntries];
    char m_CurrentDir[192];
    boolean m_LastUpPressed;
    boolean m_LastDownPressed;
    boolean m_LastPageUpPressed;
    boolean m_LastPageDownPressed;
    unsigned m_UpHoldFrames;
    unsigned m_DownHoldFrames;
    unsigned m_ContentStamp;
};

#endif
