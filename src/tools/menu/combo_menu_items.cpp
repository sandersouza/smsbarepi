#include "combo_menu_items.h"
#include "combo_locale.h"
#include "backend/runtime/runtime.h"

#ifndef SMSBARE_ONE_ROM_MODE
#define SMSBARE_ONE_ROM_MODE 0
#endif
#ifndef SMSBARE_BOOT_MODE_LOCKED
#define SMSBARE_BOOT_MODE_LOCKED 0
#endif
#ifndef SMSBARE_PATREON_TAG
#define SMSBARE_PATREON_TAG ""
#endif
#ifndef SMSBARE_ENABLE_DEBUG_OVERLAY
#define SMSBARE_ENABLE_DEBUG_OVERLAY 0
#endif

static void BuildPatreonLabel(char *dst, unsigned dst_size, const char *prefix, const char *tag)
{
    if (dst == 0 || dst_size == 0u)
    {
        return;
    }
    dst[0] = '\0';

    if (prefix == 0)
    {
        prefix = "-- Patreon --";
    }
    if (tag == 0 || tag[0] == '\0')
    {
        return;
    }

    unsigned i = 0u;
    while (prefix[i] != '\0' && i + 1u < dst_size)
    {
        dst[i] = prefix[i];
        ++i;
    }
    if (i > 0u && i + 1u < dst_size)
    {
        dst[i++] = ' ';
    }

    unsigned copied = 0u;
    while (tag[copied] != '\0' && copied < 20u && i + 1u < dst_size)
    {
        dst[i++] = tag[copied++];
    }
    dst[i] = '\0';
}

static boolean MenuProfileSupported(unsigned profile)
{
    return profile == 0u ? TRUE : FALSE;
}

static unsigned MenuSupportedProfileCount(void)
{
    const unsigned count = backend_machine_profile_count();
    unsigned supported = 0u;
    for (unsigned i = 0u; i < count; ++i)
    {
        if (MenuProfileSupported(i))
        {
            ++supported;
        }
    }
    return supported;
}

static unsigned AudioGainUiLevelFromPercent(unsigned percent)
{
    if (percent > 100u)
    {
        percent = 100u;
    }
    return (percent * 9u + 50u) / 100u;
}

static unsigned MenuProcessorModeClamp(unsigned mode)
{
    return mode == 1u ? 1u : 0u;
}

static unsigned MenuScalePercentClamp(unsigned percent)
{
    if (percent <= 200u)
    {
        return 200u;
    }
    if (percent <= 250u)
    {
        return 250u;
    }
    return 300u;
}

static const char *MenuScaleModeLabel(unsigned percent)
{
    switch (MenuScalePercentClamp(percent))
    {
    case 200u: return "1x";
    case 250u: return "2x";
    default: return "3x";
    }
}

static const char *MenuProcessorModeLabel(unsigned mode)
{
    return MenuProcessorModeClamp(mode) == 1u ? "6309" : "6809";
}

static void BuildLabelFlag(char *dst, unsigned dst_size, const char *label, const char *flag)
{
    if (dst == 0 || dst_size == 0u)
    {
        return;
    }
    if (label == 0)
    {
        label = "";
    }
    if (flag == 0)
    {
        flag = "";
    }

    unsigned i = 0u;
    while (label[i] != '\0' && i + 1u < dst_size)
    {
        dst[i] = label[i];
        ++i;
    }
    if (i + 1u < dst_size) dst[i++] = ' ';
    if (i + 1u < dst_size) dst[i++] = '[';
    for (unsigned j = 0u; flag[j] != '\0' && i + 1u < dst_size; ++j)
    {
        dst[i++] = flag[j];
    }
    if (i + 1u < dst_size) dst[i++] = ']';
    dst[i] = '\0';
}

const TComboMenuItem *combo_menu_root_items_get(unsigned language, unsigned boot_mode, unsigned *count)
{
    static TComboMenuItem root_items[16];
    static char patreon_label[48];
    static char patreon_tag_line[32];
    const TComboLocaleStrings *locale = combo_locale_get(language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }

    unsigned item_count = 0u;
#define ADD_ROOT_ITEM(lbl, act, sel) \
    do { \
        root_items[item_count].label = (lbl); \
        root_items[item_count].action = (act); \
        root_items[item_count].redraw_policy = ComboMenuRedrawMenuOnly; \
        root_items[item_count++].selectable = (sel); \
    } while (0)

    ADD_ROOT_ITEM(locale->root_load_cart, ComboMenuActionCartridgeA, TRUE);
    ADD_ROOT_ITEM("", ComboMenuActionSeparator, FALSE);
    ADD_ROOT_ITEM(locale->root_save_state, ComboMenuActionSaveState, TRUE);
    ADD_ROOT_ITEM(locale->root_load_state, ComboMenuActionLoadState, TRUE);
    ADD_ROOT_ITEM("", ComboMenuActionSeparator, FALSE);
    ADD_ROOT_ITEM(locale->root_settings, ComboMenuActionSettings, TRUE);
    ADD_ROOT_ITEM("", ComboMenuActionSeparator, FALSE);
    (void) boot_mode;
    ADD_ROOT_ITEM(locale->root_hard_reset, ComboMenuActionHardReset, TRUE);
    BuildPatreonLabel(patreon_label, sizeof(patreon_label), locale->root_patreon_prefix, SMSBARE_PATREON_TAG);
    if (patreon_label[0] != '\0')
    {
        BuildPatreonLabel(patreon_tag_line, sizeof(patreon_tag_line), "", SMSBARE_PATREON_TAG);
        ADD_ROOT_ITEM("", ComboMenuActionNone, FALSE);
        ADD_ROOT_ITEM(locale->root_patreon_prefix, ComboMenuActionNone, FALSE);
        ADD_ROOT_ITEM(patreon_tag_line, ComboMenuActionNone, FALSE);
    }
#undef ADD_ROOT_ITEM

    if (count != 0)
    {
        *count = item_count;
    }
    return root_items;
}

static const TComboMenuItem *BuildDualMediaItems(const char *left_tag,
                                                 const char *right_tag,
                                                 const char *left_name,
                                                 const char *right_name,
                                                 const char *empty_tag,
                                                 TComboMenuAction left_action,
                                                 TComboMenuAction right_action,
                                                 unsigned *count)
{
    static TComboMenuItem items[2];
    static char left_label[32];
    static char right_label[32];
    static const unsigned kMediaValueWidth = 16u;

    if (left_tag == 0 || left_tag[0] == '\0') left_tag = "A";
    if (right_tag == 0 || right_tag[0] == '\0') right_tag = "B";
    if (empty_tag == 0 || empty_tag[0] == '\0') empty_tag = "EMPTY";

    if (left_name == 0 || left_name[0] == '\0')
    {
        left_name = empty_tag;
    }
    if (right_name == 0 || right_name[0] == '\0')
    {
        right_name = empty_tag;
    }

    unsigned c = 0u;
    for (unsigned i = 0u; left_tag[i] != '\0' && c + 1u < sizeof(left_label); ++i)
    {
        left_label[c++] = left_tag[i];
    }
    if (c + 1u < sizeof(left_label))
    {
        left_label[c++] = ':';
    }
    left_label[c++] = '\t';
    unsigned n = 0u;
    for (; left_name[n] != '\0' && n < kMediaValueWidth && c + 1u < sizeof(left_label); ++n)
    {
        left_label[c++] = left_name[n];
    }
    while (n < kMediaValueWidth && c + 1u < sizeof(left_label))
    {
        left_label[c++] = ' ';
        ++n;
    }
    left_label[c] = '\0';

    c = 0u;
    for (unsigned i = 0u; right_tag[i] != '\0' && c + 1u < sizeof(right_label); ++i)
    {
        right_label[c++] = right_tag[i];
    }
    if (c + 1u < sizeof(right_label))
    {
        right_label[c++] = ':';
    }
    right_label[c++] = '\t';
    n = 0u;
    for (; right_name[n] != '\0' && n < kMediaValueWidth && c + 1u < sizeof(right_label); ++n)
    {
        right_label[c++] = right_name[n];
    }
    while (n < kMediaValueWidth && c + 1u < sizeof(right_label))
    {
        right_label[c++] = ' ';
        ++n;
    }
    right_label[c] = '\0';

    items[0].label = left_label;
    items[0].action = left_action;
    items[0].redraw_policy = ComboMenuRedrawMenuOnly;
    items[0].selectable = TRUE;
    items[1].label = right_label;
    items[1].action = right_action;
    items[1].redraw_policy = ComboMenuRedrawMenuOnly;
    items[1].selectable = TRUE;

    if (count != 0)
    {
        *count = 2u;
    }
    return items;
}

const TComboMenuItem *combo_menu_load_rom_items_get(unsigned language,
                                                     const char *slot1_name,
                                                     const char *slot2_name,
                                                     unsigned *count)
{
    const TComboLocaleStrings *locale = combo_locale_get(language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }
    return BuildDualMediaItems(locale->load_rom_slot1,
                               locale->load_rom_slot2,
                               slot1_name,
                               slot2_name,
                               locale->load_rom_empty,
                               ComboMenuActionCartridgeA,
                               ComboMenuActionCartridgeB,
                               count);
}

const TComboMenuItem *combo_menu_load_disk_items_get(unsigned language,
                                                      const char *drive1_name,
                                                      const char *drive2_name,
                                                      unsigned *count)
{
    const TComboLocaleStrings *locale = combo_locale_get(language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }
    return BuildDualMediaItems(locale->load_disk_drive1,
                               locale->load_disk_drive2,
                               drive1_name,
                               drive2_name,
                               locale->load_disk_empty,
                               ComboMenuActionDiskDriveA,
                               ComboMenuActionDiskDriveB,
                               count);
}

const TComboMenuItem *combo_menu_load_cassette_items_get(unsigned language,
                                                          const char *cassette_name,
                                                          unsigned *count)
{
    static TComboMenuItem item[1];
    static char label[32];
    static const unsigned kMediaValueWidth = 16u;
    const TComboLocaleStrings *locale = combo_locale_get(language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }
    if (cassette_name == 0 || cassette_name[0] == '\0')
    {
        cassette_name = locale->load_cassette_empty;
    }

    unsigned c = 0u;
    const char *tag = locale->load_cassette_slot;
    if (tag == 0 || tag[0] == '\0') tag = "TAPE";
    for (unsigned i = 0u; tag[i] != '\0' && c + 1u < sizeof(label); ++i)
    {
        label[c++] = tag[i];
    }
    if (c + 1u < sizeof(label)) label[c++] = ':';
    label[c++] = '\t';
    unsigned n = 0u;
    for (; cassette_name[n] != '\0' && n < kMediaValueWidth && c + 1u < sizeof(label); ++n)
    {
        label[c++] = cassette_name[n];
    }
    while (n < kMediaValueWidth && c + 1u < sizeof(label))
    {
        label[c++] = ' ';
        ++n;
    }
    label[c] = '\0';

    item[0].label = label;
    item[0].action = ComboMenuActionCassette;
    item[0].redraw_policy = ComboMenuRedrawMenuOnly;
    item[0].selectable = TRUE;
    if (count != 0) *count = 1u;
    return item;
}

const TComboMenuItem *combo_menu_settings_items_get(unsigned language,
                                                     unsigned proportion_mode,
                                                     unsigned scale_percent,
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
                                                     unsigned *count)
{
    static TComboMenuItem settings_items[32];
    static char language_label[32];
    static char proportion_label[32];
    static char scale_label[32];
    static char scanline_label[32];
    static char color_artifacts_label[40];
    static char core_label[32];
    static char machine_label[32];
    static char processor_label[32];
    static char joypad_mapping_label[40];
    static char audio_gain_label[32];

    const TComboLocaleStrings *locale = combo_locale_get(language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }

    language = combo_locale_clamp_language(language);
    if (proportion_mode > 1u) proportion_mode = 0u;
    if (scanline_mode > 1u) scanline_mode = 1u;
    machine_profile = backend_machine_profile_clamp(machine_profile);
    processor_mode = MenuProcessorModeClamp(processor_mode);
    if (audio_gain_percent > 100u) audio_gain_percent = 100u;
    (void) gfx9000_enabled;
    (void) disk_rom_enabled;
    (void) rammapper_kb;
    (void) megaram_kb;
    (void) fm_music_enabled;
    (void) scc_cart_enabled;
    (void) scc_dual_cart_enabled;
    (void) scc_dual_cart_available;

    BuildLabelFlag(language_label, sizeof(language_label), locale->settings_language, combo_locale_language_code(language));
    BuildLabelFlag(proportion_label, sizeof(proportion_label), locale->settings_proportion,
                   proportion_mode == 0u ? locale->proportion_16_9 : locale->proportion_4_3);
    BuildLabelFlag(scale_label, sizeof(scale_label), locale->settings_scale, MenuScaleModeLabel(scale_percent));
    BuildLabelFlag(scanline_label, sizeof(scanline_label), locale->settings_scanlines,
                   scanline_mode == 0u ? locale->flag_off : locale->flag_on);
    BuildLabelFlag(color_artifacts_label, sizeof(color_artifacts_label), locale->settings_color_artifacts,
                   color_artifacts_enabled ? locale->flag_on : locale->flag_off);
    BuildLabelFlag(core_label, sizeof(core_label), locale->settings_core,
                   (core_name != 0 && core_name[0] != '\0') ? core_name : "sms");
    BuildLabelFlag(machine_label, sizeof(machine_label), locale->settings_machine,
                   backend_machine_profile_label(machine_profile));
    BuildLabelFlag(processor_label, sizeof(processor_label), locale->settings_processor,
                   MenuProcessorModeLabel(processor_mode));
    BuildLabelFlag(joypad_mapping_label, sizeof(joypad_mapping_label), locale->settings_joystick_map, "->");
    {
        const unsigned audio_gain_ui = AudioGainUiLevelFromPercent(audio_gain_percent);
        char audio_gain_flag[2];
        audio_gain_flag[0] = (char) ('0' + (audio_gain_ui % 10u));
        audio_gain_flag[1] = '\0';
        BuildLabelFlag(audio_gain_label, sizeof(audio_gain_label), locale->settings_audio_gain, audio_gain_flag);
    }

    unsigned item_count = 0u;
#define ADD_SETTING_ITEM(lbl, act, policy, sel) \
    do { \
        settings_items[item_count].label = (lbl); \
        settings_items[item_count].action = (act); \
        settings_items[item_count].redraw_policy = (policy); \
        settings_items[item_count++].selectable = (sel); \
    } while (0)

    const boolean show_machine = (MenuSupportedProfileCount() > 1u) ? TRUE : FALSE;
    const boolean show_scale = SMSBARE_ENABLE_DEBUG_OVERLAY ? TRUE : FALSE;
    const boolean show_processor = FALSE;
    const boolean show_color_artifacts = FALSE;
    ADD_SETTING_ITEM(language_label, ComboMenuActionCycleLanguage, ComboMenuRedrawMenuOnly, TRUE);
    ADD_SETTING_ITEM("", ComboMenuActionSeparator, ComboMenuRedrawMenuOnly, FALSE);
    ADD_SETTING_ITEM(proportion_label, ComboMenuActionCycleProportion, ComboMenuRedrawMenuAndViewport, TRUE);
    if (show_scale)
    {
        ADD_SETTING_ITEM(scale_label, ComboMenuActionOverscanChanged, ComboMenuRedrawMenuAndViewport, TRUE);
    }
    ADD_SETTING_ITEM(scanline_label, ComboMenuActionCycleScanlines, ComboMenuRedrawMenuAndViewport, TRUE);
    if (show_color_artifacts)
    {
        ADD_SETTING_ITEM(color_artifacts_label, ComboMenuActionToggleColorArtifacts, ComboMenuRedrawMenuAndViewport, TRUE);
    }
    ADD_SETTING_ITEM(audio_gain_label, ComboMenuActionAudioGainChanged, ComboMenuRedrawMenuOnly, TRUE);

    ADD_SETTING_ITEM("", ComboMenuActionSeparator, ComboMenuRedrawMenuOnly, FALSE);
    ADD_SETTING_ITEM(core_label, ComboMenuActionCycleCore, ComboMenuRedrawMenuOnly, TRUE);
    if (show_machine)
    {
        ADD_SETTING_ITEM(machine_label, ComboMenuActionCycleMachine, ComboMenuRedrawMenuOnly, TRUE);
    }
    if (show_processor)
    {
        ADD_SETTING_ITEM(processor_label, ComboMenuActionCycleProcessor, ComboMenuRedrawMenuOnly, TRUE);
    }
    ADD_SETTING_ITEM(joypad_mapping_label, ComboMenuActionOpenJoystickMap, ComboMenuRedrawMenuOnly, TRUE);

#undef ADD_SETTING_ITEM

    if (count != 0)
    {
        *count = item_count;
    }
    return settings_items;
}

const TComboMenuItem *combo_menu_joystick_map_items_get(unsigned language,
                                                        const char *up_code,
                                                        const char *down_code,
                                                        const char *left_code,
                                                        const char *right_code,
                                                        const char *button1_code,
                                                        const char *button2_code,
                                                        unsigned *count)
{
    static TComboMenuItem items[8];
    static char up_label[48];
    static char down_label[48];
    static char left_label[48];
    static char right_label[48];
    static char button1_label[48];
    static char button2_label[48];

    const TComboLocaleStrings *locale = combo_locale_get(language);
    if (locale == 0)
    {
        locale = combo_locale_get(ComboLanguageEN);
    }

    BuildLabelFlag(up_label, sizeof(up_label), locale->joystick_map_up, up_code != 0 ? up_code : locale->flag_none);
    BuildLabelFlag(down_label, sizeof(down_label), locale->joystick_map_down, down_code != 0 ? down_code : locale->flag_none);
    BuildLabelFlag(left_label, sizeof(left_label), locale->joystick_map_left, left_code != 0 ? left_code : locale->flag_none);
    BuildLabelFlag(right_label, sizeof(right_label), locale->joystick_map_right, right_code != 0 ? right_code : locale->flag_none);
    BuildLabelFlag(button1_label, sizeof(button1_label), locale->joystick_map_button1, button1_code != 0 ? button1_code : locale->flag_none);
    BuildLabelFlag(button2_label, sizeof(button2_label), locale->joystick_map_button2, button2_code != 0 ? button2_code : locale->flag_none);

    items[0].label = up_label;
    items[1].label = down_label;
    items[2].label = left_label;
    items[3].label = right_label;
    items[4].label = button1_label;
    items[5].label = button2_label;
    for (unsigned i = 0u; i < 6u; ++i)
    {
        items[i].action = ComboMenuActionNone;
        items[i].selectable = TRUE;
        items[i].redraw_policy = ComboMenuRedrawMenuOnly;
    }

    items[6].label = "";
    items[6].action = ComboMenuActionNone;
    items[6].selectable = FALSE;
    items[6].redraw_policy = ComboMenuRedrawMenuOnly;

    items[7].label = locale->joystick_map_return;
    items[7].action = ComboMenuActionNone;
    items[7].selectable = FALSE;
    items[7].redraw_policy = ComboMenuRedrawMenuOnly;

    if (count != 0)
    {
        *count = 8u;
    }
    return items;
}
