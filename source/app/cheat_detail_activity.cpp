/*
    thomaz — cheat detail activity implementation.
*/

#include "app/cheat_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <set>
#include <string>

#include "core/cheat_txt.hpp"
#include "platform/cheat_store.hpp"

using namespace brls::literals;

namespace thomaz {

CheatDetailActivity::CheatDetailActivity(InstalledTitle title, IHttpClient* http)
    : title(std::move(title)), http(http)
{
}

CheatDetailActivity::~CheatDetailActivity()
{
    *this->alive = false; // tell any in-flight fetch callback to bail
}

void CheatDetailActivity::onContentAvailable()
{
    if (auto* gameTitle = (brls::Label*)this->getView("gameTitle"))
        gameTitle->setText(this->title.name);

    // Kick the (blocking) download onto a worker thread; the spinner + "loading"
    // status from the XML stay visible until the result comes back.
    InstalledTitle titleCopy = this->title;     // copy: worker must not touch `this`
    IHttpClient* client      = this->http;       // owned by main(), app-lifetime
    auto alive               = this->alive;      // shared liveness flag

    brls::async([this, titleCopy, client, alive]() {
        core::UrlFetcher fetch = [client](const std::string& url) -> std::optional<std::string> {
            HttpResponse r = client->get(url);
            if (!r.ok())
                return std::nullopt;
            return r.body;
        };
        core::FetchResult result =
            core::fetch_cheat_set(titleCopy.title_id, titleCopy.version, fetch);

        // Back to the UI thread to mutate views.
        brls::sync([this, alive, result]() {
            if (!alive->load())
                return; // activity was popped while we were downloading
            this->populate(result);
        });
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
            [this](brls::View*) { this->save(); return true; },
            false);
    }
}

void CheatDetailActivity::save()
{
    if (!this->loaded)
        return;

    std::set<std::string> enabled;
    for (const auto& [name, cell] : this->toggles)
        if (cell->isOn())
            enabled.insert(name);

    std::string body = core::serialize_txt(this->cheatSet.cheats, enabled);

    if (write_text_file(this->cheatSet.sd_path, body))
        brls::Application::notify("thomaz/cheats/saved"_i18n);
    else
        brls::Application::notify("thomaz/cheats/save_failed"_i18n);
}

} // namespace thomaz
