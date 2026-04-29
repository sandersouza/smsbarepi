#include "combo_locale.h"

extern const TComboLocaleStrings g_ComboLocalePT;
extern const TComboLocaleStrings g_ComboLocaleEN;
extern const TComboLocaleStrings g_ComboLocaleES;

unsigned combo_locale_clamp_language(unsigned language)
{
    return (language <= ComboLanguageES) ? language : ComboLanguageEN;
}

const char *combo_locale_language_code(unsigned language)
{
    switch (combo_locale_clamp_language(language))
    {
    case ComboLanguagePT: return "PT";
    case ComboLanguageES: return "ES";
    case ComboLanguageEN:
    default: return "EN";
    }
}

const TComboLocaleStrings *combo_locale_get(unsigned language)
{
    switch (combo_locale_clamp_language(language))
    {
    case ComboLanguagePT: return &g_ComboLocalePT;
    case ComboLanguageES: return &g_ComboLocaleES;
    case ComboLanguageEN:
    default: return &g_ComboLocaleEN;
    }
}
