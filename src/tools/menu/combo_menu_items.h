#ifndef SMSBARE_CIRCLE_SMSBARE_MENU_ITEMS_H
#define SMSBARE_CIRCLE_SMSBARE_MENU_ITEMS_H

#include <circle/types.h>

typedef enum {
    ComboDebugOverlayOff = 0,
    ComboDebugOverlayMax = 1,
    ComboDebugOverlayMin = 2
} TComboDebugOverlayMode;

enum TComboMenuAction {
    ComboMenuActionNone = 0,
    ComboMenuActionSeparator,
    ComboMenuActionCartridgeA,
    ComboMenuActionCartridgeB,
    ComboMenuActionDiskDriveA,
    ComboMenuActionDiskDriveB,
    ComboMenuActionCassette,
    ComboMenuActionSettings,
    ComboMenuActionCycleLanguage,
    ComboMenuActionCycleLanguagePrev,
    ComboMenuActionToggleOverscan,
    ComboMenuActionCycleScanlines,
    ComboMenuActionCycleScanlinesPrev,
    ComboMenuActionToggleColorArtifacts,
    ComboMenuActionToggleGfx9000,
    ComboMenuActionToggleDiskRom,
    ComboMenuActionCycleCore,
    ComboMenuActionCycleCorePrev,
    ComboMenuActionCycleMachine,
    ComboMenuActionCycleMachinePrev,
    ComboMenuActionCycleProcessor,
    ComboMenuActionCycleProcessorPrev,
    ComboMenuActionCycleRamMapper,
    ComboMenuActionCycleRamMapperPrev,
    ComboMenuActionCycleMegaRam,
    ComboMenuActionCycleMegaRamPrev,
    ComboMenuActionOpenJoystickMap,
    ComboMenuActionApplyJoystickMap,
    ComboMenuActionToggleFmMusic,
    ComboMenuActionToggleSccCart,
    ComboMenuActionAudioGainChanged,
    ComboMenuActionSaveState,
    ComboMenuActionLoadState,
    ComboMenuActionSoftReset,
    ComboMenuActionHardReset
};

enum TComboMenuRedrawPolicy {
    ComboMenuRedrawMenuOnly = 0,
    ComboMenuRedrawMenuAndViewport = 1
};

typedef struct {
    const char *label;
    TComboMenuAction action;
    boolean selectable;
    TComboMenuRedrawPolicy redraw_policy;
} TComboMenuItem;

const TComboMenuItem *combo_menu_root_items_get(unsigned language, unsigned boot_mode, unsigned *count);
const TComboMenuItem *combo_menu_load_rom_items_get(unsigned language,
                                                     const char *slot1_name,
                                                     const char *slot2_name,
                                                     unsigned *count);
const TComboMenuItem *combo_menu_load_disk_items_get(unsigned language,
                                                      const char *drive1_name,
                                                      const char *drive2_name,
                                                      unsigned *count);
const TComboMenuItem *combo_menu_load_cassette_items_get(unsigned language,
                                                          const char *cassette_name,
                                                          unsigned *count);
const TComboMenuItem *combo_menu_settings_items_get(unsigned language,
                                                     boolean overscan_enabled,
                                                     unsigned scanline_mode,
                                                     boolean color_artifacts_enabled,
                                                     boolean gfx9000_enabled,
                                                     boolean disk_rom_enabled,
                                                     const char *core_name,
                                                     unsigned machine_profile,
                                                     unsigned processor_mode,
                                                     unsigned rammapper_kb,
                                                     unsigned megaram_kb,
                                                     boolean fm_music_enabled,
                                                     boolean scc_cart_enabled,
                                                     boolean scc_dual_cart_enabled,
                                                     boolean scc_dual_cart_available,
                                                     unsigned audio_gain_percent,
                                                     unsigned *count);
const TComboMenuItem *combo_menu_joystick_map_items_get(unsigned language,
                                                        const char *up_code,
                                                        const char *down_code,
                                                        const char *left_code,
                                                        const char *right_code,
                                                        const char *button1_code,
                                                        const char *button2_code,
                                                        unsigned *count);

#endif
