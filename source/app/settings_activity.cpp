/*
    thomaz — settings activity implementation.
*/

#include "app/settings_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <string>
#include <vector>

#include "app/version.hpp"
#include "core/db_paths.hpp"
#include "core/update.hpp"
#include "platform/app_settings.hpp"
#include "platform/cheat_store.hpp"
#include "platform/self_update.hpp"
#include "platform/feed/auth_store.hpp"

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
    row->setCornerRadius(12.0f);
    row->setAlignItems(brls::AlignItems::CENTER);
    row->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2

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


void SettingsActivity::onContentAvailable()
{
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);

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

    // --- Log out of the community feed --------------------------------------
    // Clears the persisted session; the next post/like/comment will prompt login.
    auto* logoutRow = makeActionRow("thomaz/auth/logout"_i18n);
    logoutRow->registerClickAction([](brls::View*) {
        clear_session();
        brls::Application::notify("thomaz/auth/logout"_i18n);
        return true;
    });
    logoutRow->addGestureRecognizer(new brls::TapGestureRecognizer(logoutRow));
    listBox->addView(logoutRow);
}

void SettingsActivity::checkForUpdate(brls::Label* status)
{
    if (this->busy)
        return;
    this->busy = true;
    status->setText("thomaz/update/checking"_i18n);

    IHttpClient* client = this->http;
    auto cancelled      = this->cancelledFlag();

    auto results = std::make_shared<std::pair<bool, core::ReleaseInfo>>(); // (ok, rel)
    this->runAsync(
        [client, results, cancelled]() {
            HttpRequest req{};
            req.url           = core::github_latest_release_url();
            req.cancelled     = cancelled;
            HttpResponse r    = client->request(req);
            results->first    = r.ok();
            results->second   = results->first ? core::parse_latest_release(r.body, "thomaz.nro")
                                               : core::ReleaseInfo{};
        },
        [this, results, status]() {
            this->busy = false;

            if (!results->first)
            {
                status->setText("thomaz/update/error"_i18n);
                return;
            }
            const core::ReleaseInfo& rel = results->second;
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
                              [this, alive = this->alive, rel, status]() {
                                  if (!alive->load()) return;
                                  this->installUpdate(rel, status);
                              });
            dialog->addButton("thomaz/update/confirm_no"_i18n, []() {});
            dialog->open();
        });
}

void SettingsActivity::installUpdate(const core::ReleaseInfo& release, brls::Label* status)
{
    if (this->busy)
        return;
    this->busy = true;
    status->setText("thomaz/update/downloading"_i18n);

    std::string url    = release.nro_url;
    std::string target = update_target_path();
    auto cancelled     = this->cancelledFlag();

    auto ok  = std::make_shared<bool>(false);
    auto msg = std::make_shared<std::string>();
    this->runAsync(
        [url, target, ok, msg, cancelled]() {
            *ok = apply_downloaded_update(url, target, msg.get(), cancelled);
        },
        [this, ok, msg, status]() {
            this->busy = false;
            if (*ok) {
                status->setText("thomaz/update/done"_i18n);
            } else {
                // Show the error detail in the status line so the user (and
                // nxlink/log) can see what actually went wrong, not just a
                // generic "falha ao atualizar".
                std::string detail = msg->empty() ? "thomaz/update/failed"_i18n
                                                  : ("thomaz/update/failed"_i18n + ": " + *msg);
                status->setText(detail);
            }
        });
}

void SettingsActivity::refreshDatabase(brls::Label* status)
{
    if (this->busy)
        return;
    this->busy = true;
    status->setText("thomaz/db/refreshing"_i18n);

    IHttpClient* client = this->http;
    auto cancelled      = this->cancelledFlag();

    auto wrote = std::make_shared<bool>(false);
    this->runAsync(
        [client, wrote, cancelled]() {
            HttpRequest req{};
            req.url        = core::db_index_url();
            req.cancelled  = cancelled;
            HttpResponse r = client->request(req);
            *wrote         = r.ok() && write_text_file(index_cache_path(), r.body);
        },
        [this, wrote, status]() {
            this->busy = false;
            status->setText(*wrote ? "thomaz/db/refresh_done"_i18n
                                   : "thomaz/db/refresh_failed"_i18n);
        });
}

} // namespace thomaz
