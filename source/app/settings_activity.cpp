/*
    thomaz — settings activity implementation.
*/

#include "app/settings_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <string>
#include <vector>

#include "app/version.hpp"
#include "core/db_paths.hpp"
#include "core/update.hpp"
#include "platform/app_settings.hpp"
#include "platform/cheat_store.hpp"
#include "platform/self_update.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {

// A focusable settings action row (rounded surface box + a label).
brls::Box* makeActionRow(const std::string& text)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setHeight(56.0f);
    row->setFocusable(true);
    row->setMarginTop(10.0f);
    row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
    row->setCornerRadius(8.0f);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setBackgroundColor(nvgRGB(0x2A, 0x2D, 0x36));

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(16.0f);
    label->setGrow(1.0f);
    row->addView(label);
    return row;
}

} // namespace

SettingsActivity::SettingsActivity(IHttpClient* http)
    : http(http)
{
}

SettingsActivity::~SettingsActivity()
{
    *this->alive = false;
}

void SettingsActivity::onContentAvailable()
{
    auto* listBox = (brls::Box*)this->getView("settingsListBox");
    if (!listBox)
        return;

    // --- Language selector ---------------------------------------------------
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

    // Shared status line for the network actions below.
    auto* status = new brls::Label();
    status->setFontSize(14.0f);
    status->setMarginTop(16.0f);
    status->setText("thomaz/update/current_version"_i18n + std::string(THOMAZ_VERSION));

    // --- Check for app updates ----------------------------------------------
    auto* updateRow = makeActionRow("thomaz/update/check"_i18n);
    updateRow->registerClickAction([this, status](brls::View*) {
        this->checkForUpdate(status);
        return true;
    });
    updateRow->addGestureRecognizer(new brls::TapGestureRecognizer(updateRow));
    listBox->addView(updateRow);

    // --- Refresh cheats database --------------------------------------------
    auto* dbRow = makeActionRow("thomaz/db/refresh"_i18n);
    dbRow->registerClickAction([this, status](brls::View*) {
        this->refreshDatabase(status);
        return true;
    });
    dbRow->addGestureRecognizer(new brls::TapGestureRecognizer(dbRow));
    listBox->addView(dbRow);

    listBox->addView(status);
}

void SettingsActivity::checkForUpdate(brls::Label* status)
{
    if (this->busy)
        return;
    this->busy = true;
    status->setText("thomaz/update/checking"_i18n);

    IHttpClient* client = this->http;
    auto alive          = this->alive;

    brls::async([this, client, alive, status]() {
        HttpResponse r = client->get(core::github_latest_release_url());
        bool ok        = r.ok();
        core::ReleaseInfo rel = ok ? core::parse_latest_release(r.body, "thomaz.nro")
                                   : core::ReleaseInfo{};

        brls::sync([this, alive, status, ok, rel]() {
            if (!alive->load())
                return;
            this->busy = false;

            if (!ok)
            {
                status->setText("thomaz/update/error"_i18n);
                return;
            }
            if (!rel.valid)
            {
                status->setText("thomaz/update/none"_i18n);
                return;
            }
            if (!core::is_newer_version(rel.tag, THOMAZ_VERSION))
            {
                status->setText("thomaz/update/up_to_date"_i18n);
                return;
            }

            // Newer version available — ask before installing.
            std::string msg = "thomaz/update/confirm_pre"_i18n + rel.tag +
                              "thomaz/update/confirm_post"_i18n;
            auto* dialog = new brls::Dialog(msg);
            dialog->addButton("thomaz/update/confirm_yes"_i18n,
                              [this, rel, status]() { this->installUpdate(rel, status); });
            dialog->addButton("thomaz/update/confirm_no"_i18n, []() {});
            dialog->open();
        });
    });
}

void SettingsActivity::installUpdate(const core::ReleaseInfo& release, brls::Label* status)
{
    if (this->busy)
        return;
    this->busy = true;
    status->setText("thomaz/update/downloading"_i18n);

    IHttpClient* client = this->http;
    auto alive          = this->alive;
    std::string url     = release.nro_url;
    std::string target  = update_target_path();

    brls::async([this, client, alive, status, url, target]() {
        HttpResponse r = client->get(url);
        bool wrote     = r.ok() && !r.body.empty() && write_text_file(target, r.body);

        brls::sync([this, alive, status, wrote]() {
            if (!alive->load())
                return;
            this->busy = false;
            status->setText(wrote ? "thomaz/update/done"_i18n : "thomaz/update/failed"_i18n);
        });
    });
}

void SettingsActivity::refreshDatabase(brls::Label* status)
{
    if (this->busy)
        return;
    this->busy = true;
    status->setText("thomaz/db/refreshing"_i18n);

    IHttpClient* client = this->http;
    auto alive          = this->alive;

    brls::async([this, client, alive, status]() {
        HttpResponse r = client->get(core::db_index_url());
        bool wrote     = r.ok() && write_text_file(index_cache_path(), r.body);

        brls::sync([this, alive, status, wrote]() {
            if (!alive->load())
                return;
            this->busy = false;
            status->setText(wrote ? "thomaz/db/refresh_done"_i18n
                                  : "thomaz/db/refresh_failed"_i18n);
        });
    });
}

} // namespace thomaz
