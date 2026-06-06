/*
    thomaz — cheat detail activity implementation.
*/

#include "app/cheat_detail_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/game_panel.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <set>
#include <string>

#include "core/cheat_txt.hpp"
#include "platform/cheat_store.hpp"
#include "platform/atmosphere_config.hpp"

using namespace brls::literals;

namespace thomaz {

CheatDetailActivity::CheatDetailActivity(InstalledTitle title, IHttpClient* http)
    : title(std::move(title)), http(http)
{
}

void CheatDetailActivity::onContentAvailable()
{
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);
    install_help_action(this, "cheatFrame", "thomaz/help/cheats");

    populate_game_panel(this, this->title);

    // Kick the (blocking) download onto a worker thread; the spinner + "loading"
    // status from the XML stay visible until the result comes back.
    InstalledTitle titleCopy = this->title;     // copy: worker must not touch `this`
    IHttpClient* client      = this->http;       // owned by main(), app-lifetime

    auto result    = std::make_shared<core::FetchResult>();
    auto cancelled = this->cancelledFlag();
    this->runAsync(
        [titleCopy, client, result, cancelled]() {
            core::UrlFetcher fetch = [client, cancelled](const std::string& url) -> std::optional<std::string> {
                HttpRequest req;
                req.url       = url;
                req.cancelled = cancelled;
                HttpResponse r = client->request(req);
                // Distinguish "couldn't reach the server" from "server answered, but
                // this game isn't in the db". status 0 == transport/connection failure
                // -> nullopt -> NetworkError ("check your connection"). A reachable
                // non-200 (typically 404: most games simply aren't in switch-cheats-db)
                // is NOT a connection problem: return an empty doc so the resolver
                // ends up at NotInDb ("no cheats for this game") instead of scaring
                // the user with a bogus network error.
                if (r.status == 0)
                    return std::nullopt;
                if (!r.ok())
                    return std::string{};
                return r.body;
            };
            *result = core::fetch_cheat_set(titleCopy.title_id, titleCopy.version, fetch);
        },
        [this, result]() {
            this->populate(*result);
        });
}

void CheatDetailActivity::populate(const core::FetchResult& result)
{
    auto* statusLabel = (brls::Label*)this->getView("statusLabel");
    auto* listBox     = (brls::Box*)this->getView("cheatListBox");

    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE); // download finished

    if (result.status == core::FetchStatus::NetworkError)
    {
        if (statusLabel)
            statusLabel->setText("thomaz/cheats/error_network"_i18n);
        return;
    }
    if (result.status == core::FetchStatus::NotInDb)
    {
        if (statusLabel)
            statusLabel->setText("thomaz/cheats/none"_i18n);
        return;
    }

    // Ok: we have cheats for a resolved build_id.
    this->cheatSet = result.set;
    this->loaded   = true;

    // Pre-check toggles from a previously-saved file, if any.
    std::set<std::string> alreadyEnabled;
    if (auto existing = read_text_file(this->cheatSet.sd_path))
        alreadyEnabled = core::enabled_cheat_names(*existing);

    // Status line: warn if we fell back to an older build's cheats.
    if (statusLabel)
    {
        if (this->cheatSet.resolution.source == core::Resolution::Source::FallbackOlderBuild)
            statusLabel->setText("thomaz/cheats/fallback"_i18n);
        else
            statusLabel->setVisibility(brls::Visibility::GONE);
    }

    if (listBox)
    {
        for (const auto& cheat : this->cheatSet.cheats)
        {
            if (cheat.is_master)
                continue; // master codes are always applied; not user-toggleable

            auto* cell = new brls::BooleanCell();
            // Auto-save whenever a toggle flips (no separate save step needed
            // for touch/mouse users). The X action below stays for controllers.
            cell->init(cheat.name, alreadyEnabled.count(cheat.name) > 0,
                       [this](bool) { this->save(); });
            // Respond to touch (Switch) and mouse (desktop), not just the A button.
            cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
            listBox->addView(cell);
            this->toggles.emplace_back(cheat.name, cell);
        }
    }

    // Bind the save/apply action (shown in the bottom bar).
    if (auto* frame = this->getView("cheatFrame"))
    {
        frame->registerAction(
            "thomaz/cheats/save"_i18n, brls::BUTTON_X,
            [this](brls::View*) { this->save(/*showApplyInfo=*/true); return true; },
            false);
    }
}

void CheatDetailActivity::save(bool showApplyInfo)
{
    if (!this->loaded)
        return;

    std::set<std::string> enabled;
    for (const auto& [name, cell] : this->toggles)
        if (cell->isOn())
            enabled.insert(name);

    std::string body = core::serialize_txt(this->cheatSet.cheats, enabled);

    bool ok = write_text_file(this->cheatSet.sd_path, body);

    if (!ok)
    {
        brls::Application::notify("thomaz/cheats/save_failed"_i18n);
        return;
    }

    // On the explicit "save and apply" action, with at least one cheat enabled,
    // a fleeting toast isn't enough — guide the user on what actually makes the
    // cheat take effect. Auto-saves (toggle flips) and "nothing enabled" stay a
    // quiet toast.
    if (showApplyInfo && !enabled.empty())
    {
        // Regular '[cheat]' entries load OFF unless Atmosphère is told to enable
        // cheats by default. Ensure that setting so the user doesn't have to flip
        // each cheat on in an overlay. If we had to change it, the console must
        // reboot once before it applies; otherwise a game relaunch is enough.
        CheatsDefaultResult cfg = ensure_cheats_enabled_by_default();
        const char* bodyKey = (cfg == CheatsDefaultResult::Enabled)
                                  ? "thomaz/cheats/apply_body_reboot"
                                  : "thomaz/cheats/apply_body";

        auto* dialog = new brls::Dialog(brls::getStr(bodyKey));
        dialog->addButton("thomaz/cheats/apply_ok"_i18n, []() {});
        dialog->open();
    }
    else
    {
        brls::Application::notify("thomaz/cheats/saved"_i18n);
    }
}

} // namespace thomaz
