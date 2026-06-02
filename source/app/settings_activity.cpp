/*
    thomaz — settings activity implementation.
*/

#include "app/settings_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <string>
#include <vector>

#include "platform/app_settings.hpp"

using namespace brls::literals;

namespace thomaz {

void SettingsActivity::onContentAvailable()
{
    auto* listBox = (brls::Box*)this->getView("settingsListBox");
    if (!listBox)
        return;

    // Language selector. Index maps to a stored locale string.
    static const std::vector<std::string> kLocales = { "auto", "pt-BR", "en-US" };
    std::vector<std::string> options = {
        "thomaz/settings/lang_auto"_i18n,
        "Português (Brasil)",
        "English",
    };

    std::string current = load_locale();
    int selected = 0;
    for (size_t i = 0; i < kLocales.size(); ++i)
        if (kLocales[i] == current)
            selected = (int)i;

    auto* langCell = new brls::SelectorCell();
    langCell->init(
        "thomaz/settings/language"_i18n, options, selected,
        [](int index) {
            if (index >= 0 && index < (int)kLocales.size())
            {
                save_locale(kLocales[index]);
                brls::Application::notify("thomaz/settings/saved"_i18n);
            }
        });
    langCell->addGestureRecognizer(new brls::TapGestureRecognizer(langCell));
    listBox->addView(langCell);
}

} // namespace thomaz
