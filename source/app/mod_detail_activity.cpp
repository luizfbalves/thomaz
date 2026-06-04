/*
    thomaz — mod detail activity implementation.
*/

#include "app/mod_detail_activity.hpp"
#include "app/app_header.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <optional>
#include <string>
#include <utility>

#include "core/mods/mod_browse.hpp"
#include "core/mods/mod_paths.hpp"
#include "platform/http_client.hpp"
#include "platform/mods/mod_actions.hpp"
#include "platform/mods/mod_download.hpp"
#include "platform/title.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {

// Human-readable byte size: simple KB/MB string.
std::string human_size(std::uint64_t bytes)
{
    if (bytes >= 1024ull * 1024ull)
        return std::to_string(bytes / (1024ull * 1024ull)) + " MB";
    if (bytes >= 1024ull)
        return std::to_string(bytes / 1024ull) + " KB";
    return std::to_string(bytes) + " B";
}

// filename without its final extension ("Mod.v2.zip" -> "Mod.v2").
// Leading-dot names (e.g. ".zip") are returned unchanged.
std::string strip_extension(const std::string& name)
{
    std::string::size_type dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0)
        return name;
    return name.substr(0, dot);
}

} // namespace

ModDetailActivity::ModDetailActivity(InstalledTitle title, std::uint64_t mod_id, IHttpClient* http,
                                     bool manual_search)
    : title(std::move(title)), modId(mod_id), http(http), manualSearch(manual_search)
{
}

ModDetailActivity::~ModDetailActivity()
{
    *this->alive = false; // tell an in-flight UI callback to bail
}

void ModDetailActivity::onContentAvailable()
{
    install_header_username(this);

    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);

    // Resolve the mod's files on a worker thread; the spinner stays up until done.
    std::uint64_t mod_id = this->modId;
    IHttpClient* client  = this->http; // owned by main(), app-lifetime
    auto alive           = this->alive;

    brls::async([this, mod_id, client, alive]() {
        core::UrlFetcher fetch = [client](const std::string& url) -> std::optional<std::string> {
            HttpResponse r = client->get(url);
            return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
        };
        core::ResolveResult res = core::resolve_mod_files(mod_id, fetch);

        brls::sync([this, alive, res]() {
            if (!alive->load())
                return; // activity was popped while we were loading
            this->populate(res);
        });
    });
}

void ModDetailActivity::populate(const core::ResolveResult& result)
{
    auto* filesBox   = (brls::Box*)this->getView("filesBox");
    auto* emptyLabel = (brls::Label*)this->getView("emptyLabel");

    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE); // load finished

    if (result.status == core::ResolveStatus::NetworkError)
    {
        brls::Application::notify("mods/search_error"_i18n);
        return;
    }
    if (result.status == core::ResolveStatus::NotFound)
    {
        if (emptyLabel)
        {
            emptyLabel->setText(result.error.empty() ? "mods/no_files"_i18n : result.error);
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        }
        return;
    }
    if (result.files.empty())
    {
        if (emptyLabel)
        {
            emptyLabel->setText("mods/no_files"_i18n);
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        }
        return;
    }

    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);

    if (!filesBox)
        return;

    filesBox->clearViews();

    for (const auto& file : result.files)
    {
        core::ModFile f = file;

        auto* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(48.0f);
        row->setFocusable(true);
        row->setMarginBottom(8.0f);
        row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        row->setCornerRadius(12.0f);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23)); // surface_1

        auto* nameLabel = new brls::Label();
        nameLabel->setText(f.filename);
        nameLabel->setFontSize(16.0f);
        nameLabel->setGrow(1.0f);
        row->addView(nameLabel);

        auto* sizeLabel = new brls::Label();
        sizeLabel->setText(human_size(f.filesize));
        sizeLabel->setFontSize(14.0f);
        sizeLabel->setMarginLeft(12.0f);
        row->addView(sizeLabel);

        row->registerClickAction([this, f](brls::View*) {
            // Defer to avoid mutating views mid-event; confirmAndDownload may open
            // a dialog, and startDownload kicks an async job. Keep the M1
            // deferred-handler discipline.
            brls::sync([this, f]() { this->confirmAndDownload(f); });
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        filesBox->addView(row);
    }
}

void ModDetailActivity::confirmAndDownload(const core::ModFile& file)
{
    // Auto-resolved game: the mod belongs to this game, install straight away.
    if (!this->manualSearch)
    {
        this->startDownload(file);
        return;
    }

    // Manual/global search: this mod may be for a different game, and it would
    // be staged under the *current* game. Confirm the target first.
    std::string body = "mods/confirm_install_body"_i18n; // "... {{game}}?"
    auto        pos  = body.find("{{game}}");
    if (pos != std::string::npos)
        body.replace(pos, 8, this->title.name);

    core::ModFile f      = file;
    auto          alive  = this->alive;
    brls::Dialog* dialog = new brls::Dialog(body);
    dialog->addButton("mods/download"_i18n, [this, alive, f]() {
        if (!alive->load())
            return;
        this->startDownload(f);
    });
    dialog->addButton("mods/cancel"_i18n, []() {});
    dialog->open();
}

void ModDetailActivity::startDownload(const core::ModFile& file)
{
    core::ModFile f      = file;
    std::string dest     = core::mod_staging_root() + "/_incoming/" + f.filename;
    std::string mod_name = strip_extension(f.filename);

    brls::Application::notify("mods/downloading"_i18n);

    std::uint64_t tid = this->title.title_id; // copy before async — avoids UAF if activity is popped
    auto alive        = this->alive;

    brls::async([alive, f, dest, mod_name, tid]() {
        std::string err;
        // progress nullptr is safe — download_file guards `if(progress)`.
        // A live progress bar is a future refinement.
        bool ok      = download_file(f.download_url, dest, nullptr, &err);
        bool done_ok = false;
        std::string msg;
        if (ok)
        {
            ModActionResult ir = import_archive(tid, mod_name, dest, nullptr);
            done_ok            = ir.ok;
            msg = ir.ok ? "mods/installed_ok"_i18n
                        : ("mods/download_failed"_i18n + std::string(": ") + ir.error);
        }
        else
        {
            msg = "mods/download_failed"_i18n + std::string(": ") + err;
        }

        brls::sync([alive, msg, done_ok]() {
            if (!alive->load())
                return;
            brls::Application::notify(msg);
            if (done_ok)
                brls::Application::popActivity();
        });
    });
}

} // namespace thomaz
