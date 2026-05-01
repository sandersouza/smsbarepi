#ifndef SMSBARE_CIRCLE_SMSBARE_LOCALE_H
#define SMSBARE_CIRCLE_SMSBARE_LOCALE_H

#include <circle/types.h>

enum TComboLanguage {
    ComboLanguagePT = 0,
    ComboLanguageEN = 1,
    ComboLanguageES = 2
};

typedef struct {
    const char *title_pause;
    const char *title_settings;
    const char *title_joystick_map;
    const char *title_load_rom;
    const char *title_load_disk;
    const char *title_load_cassette;
    const char *root_load_cart;
    const char *root_load_disk;
    const char *root_load_cassette;
    const char *root_save_state;
    const char *root_load_state;
    const char *root_settings;
    const char *root_soft_reset;
    const char *root_hard_reset;
    const char *root_patreon_prefix;
    const char *load_rom_slot1;
    const char *load_rom_slot2;
    const char *load_rom_empty;
    const char *load_rom_unload_hint;
    const char *load_disk_drive1;
    const char *load_disk_drive2;
    const char *load_disk_empty;
    const char *load_disk_unload_hint;
    const char *load_cassette_slot;
    const char *load_cassette_empty;
    const char *load_cassette_unload_hint;
    const char *settings_language;
    const char *settings_overscan;
    const char *settings_scanlines;
    const char *settings_color_artifacts;
    const char *settings_core;
    const char *settings_machine;
    const char *settings_processor;
    const char *settings_autofire;
    const char *settings_joystick_map;
    const char *settings_audio_gain;
    const char *joystick_map_up;
    const char *joystick_map_down;
    const char *joystick_map_left;
    const char *joystick_map_right;
    const char *joystick_map_button1;
    const char *joystick_map_button2;
    const char *joystick_map_return;
    const char *flag_off;
    const char *flag_on;
    const char *flag_dual;
    const char *flag_none;
    const char *flag_na;
} TComboLocaleStrings;

unsigned combo_locale_clamp_language(unsigned language);
const char *combo_locale_language_code(unsigned language);
const TComboLocaleStrings *combo_locale_get(unsigned language);

#endif
