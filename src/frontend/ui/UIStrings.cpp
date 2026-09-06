#include "UIStrings.h"
#include "settings/SettingsManager.h"

namespace ProyecThor::UI {

extern const UIStrings kSpanish;
extern const UIStrings kEnglish;
extern const UIStrings kPortuguese;

const UIStrings& GetUIStrings() {
    using ProyecThor::Settings::Language;
    switch (ProyecThor::Settings::SettingsManager::Get().GetSettings().general.language) {
        case Language::English:    return kEnglish;
        case Language::Portuguese: return kPortuguese;
        case Language::Spanish:
        default:                   return kSpanish;
    }
}

} // namespace ProyecThor::UI
