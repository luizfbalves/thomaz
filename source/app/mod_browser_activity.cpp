/*
    thomaz — mod browser activity implementation.
*/

#include "app/mod_browser_activity.hpp"
#include "app/app_header.hpp"
#include "app/mod_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/ime.hpp>
#include <borealis/core/thread.hpp>
#include <optional>
#include <string>
#include <utility>

#include "core/mods/mod_browse.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

using namespace brls::literals;

namespace thomaz {

ModBrowserActivity::ModBrowserActivity(InstalledTitle title, IHttpClient* http)
    : title(std::move(title)), http(http)
{
}

ModBrowserActivity::~ModBrowserActivity()
{
    *this->alive = false; // tell an in-flight UI callback to bail
}

void ModBrowserActivity::onContentAvailable()
{
    install_header_username(this);

    // Show the spinner while we resolve this game on GameBanana. Instead of
    // immediately opening the keyboard (M2 behaviour), we first try to resolve
    // the installed title to a GameBanana game_id and list ITS mods. The
    // keyboard only opens as a fallback when the game isn't found.
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);
    if (auto* emptyLabel = (brls::Label*)this->getView("emptyLabel"))
    {
        emptyLabel->setText("mods/resolving"_i18n);
        emptyLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    auto alive       = this->alive;
    IHttpClient* http = this->http; // owned by main(), app-lifetime
    std::uint64_t tid = this->title.title_id; // copy — worker must not touch `this`
    std::string name  = this->title.name;

    brls::async([this, alive, http, tid, name]() {
        core::UrlFetcher fetch = [http](const std::string& url) -> std::optional<std::string> {
            HttpResponse r = http->get(url);
            return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
        };
        core::GameResolve g = core::resolve_game(tid, name, fetch);
        core::BrowseResult mods;
        if (g.status == core::GameResolveStatus::Ok)
            mods = core::list_game_mods(g.game_id, "", 1, fetch);

        brls::sync([this, alive, g, mods]() {
            if (!alive->load())
                return; // activity was popped while we were resolving
            this->onResolved(g, mods);
        });
    });
}

void ModBrowserActivity::onResolved(const core::GameResolve& g, const core::BrowseResult& mods)
{
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE);

    if (g.status == core::GameResolveStatus::NetworkError)
    {
        brls::Application::notify("mods/search_error"_i18n);
        if (auto* emptyLabel = (brls::Label*)this->getView("emptyLabel"))
        {
            emptyLabel->setText("mods/search_error"_i18n);
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        }
        return;
    }

    if (g.status == core::GameResolveStatus::NotFound)
    {
        // Fall back to the M2 global free-text search: tell the user, then open
        // the keyboard. The callback runs on the UI thread (not a view-click
        // handler), so runGlobalSearch() may be called directly.
        this->gameId = 0;
        this->setResolvedLabel(true, "");
        if (auto* emptyLabel = (brls::Label*)this->getView("emptyLabel"))
        {
            emptyLabel->setText("mods/game_not_found"_i18n);
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        }
        brls::Application::getPlatform()->getImeManager()->openForText(
            [this, alive = this->alive](std::string q) {
                if (!alive->load()) return;
                if (q.empty())
                    return;
                this->query = q;
                this->page  = 1;
                this->runGlobalSearch();
            },
            "mods/search"_i18n, "mods/search_hint"_i18n, 64);
        return;
    }

    // Ok: resolved to a GameBanana game. Show its mods (Subfeed).
    this->gameId = g.game_id;
    this->query  = "";
    this->page   = 1;
    this->setResolvedLabel(false, g.matched_name);

    if (mods.status != core::BrowseStatus::Ok)
    {
        brls::Application::notify("mods/search_error"_i18n);
        return;
    }

    this->lastPage = mods.page;
    this->populate(mods); // populate() prepends the in-game search row in game mode
}

void ModBrowserActivity::setResolvedLabel(bool manual, const std::string& gameName)
{
    auto* lbl = (brls::Label*)this->getView("resolvedLabel");
    if (!lbl)
        return;
    if (manual)
    {
        lbl->setText("mods/manual_search_label"_i18n);
    }
    else
    {
        std::string s   = "mods/showing_game"_i18n; // "... {{game}}"
        auto        pos = s.find("{{game}}");
        if (pos != std::string::npos)
            s.replace(pos, 8, gameName);
        else
            s += " " + gameName;
        lbl->setText(s);
    }
    lbl->setVisibility(brls::Visibility::VISIBLE);
}

void ModBrowserActivity::runGameSearch(const std::string& query)
{
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);

    IHttpClient* client = this->http; // owned by main(), app-lifetime
    auto alive          = this->alive;
    std::uint64_t gid   = this->gameId; // copy — worker must not touch `this`
    std::string q       = query;
    int page            = this->page;

    brls::async([this, alive, client, gid, q, page]() {
        core::UrlFetcher fetch = [client](const std::string& url) -> std::optional<std::string> {
            HttpResponse r = client->get(url);
            return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
        };
        core::BrowseResult res = core::list_game_mods(gid, q, page, fetch);

        brls::sync([this, alive, res]() {
            if (!alive->load())
                return; // activity was popped while we were loading
            this->populate(res);
        });
    });
}

void ModBrowserActivity::runGlobalSearch()
{
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);

    IHttpClient* client = this->http; // owned by main(), app-lifetime
    auto alive          = this->alive;
    std::string q       = this->query;
    int page            = this->page;

    brls::async([this, alive, client, q, page]() {
        core::UrlFetcher fetch = [client](const std::string& url) -> std::optional<std::string> {
            HttpResponse r = client->get(url);
            return r.ok() ? std::optional<std::string>(r.body) : std::nullopt;
        };
        core::BrowseResult res = core::search_mods(q, 0, page, fetch);

        brls::sync([this, alive, res]() {
            if (!alive->load())
                return; // activity was popped while we were loading
            this->populate(res);
        });
    });
}

void ModBrowserActivity::populate(const core::BrowseResult& result)
{
    auto* resultsBox = (brls::Box*)this->getView("resultsBox");
    auto* emptyLabel = (brls::Label*)this->getView("emptyLabel");

    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE); // search finished

    if (result.status == core::BrowseStatus::NetworkError)
    {
        brls::Application::notify("mods/search_error"_i18n);
        return;
    }

    this->lastPage = result.page;

    if (!resultsBox)
        return;

    resultsBox->clearViews();

    // In game mode, prepend a row that opens the keyboard for an in-game search
    // (Subfeed text query). Re-added on every rebuild so load-more keeps it.
    if (this->gameId != 0)
    {
        auto* searchRow = new brls::Box(brls::Axis::ROW);
        searchRow->setHeight(48.0f);
        searchRow->setFocusable(true);
        searchRow->setMarginBottom(8.0f);
        searchRow->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        searchRow->setCornerRadius(12.0f);
        searchRow->setJustifyContent(brls::JustifyContent::CENTER);
        searchRow->setAlignItems(brls::AlignItems::CENTER);
        searchRow->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2

        auto* searchLabel = new brls::Label();
        searchLabel->setText("mods/game_search"_i18n);
        searchLabel->setFontSize(16.0f);
        searchRow->addView(searchLabel);

        searchRow->registerClickAction([this](brls::View*) {
            // Opening the keyboard from a click handler is fine; the rebuild
            // happens later inside the IME callback (UI thread), but wrap the
            // list rebuild in brls::sync to be safe (M2 lesson).
            brls::Application::getPlatform()->getImeManager()->openForText(
                [this, alive = this->alive](std::string q) {
                    if (!alive->load()) return;
                    if (q.empty())
                        return;
                    this->query = q;
                    this->page  = 1;
                    brls::sync([this, q]() { this->runGameSearch(q); });
                },
                "mods/game_search"_i18n, "mods/search_hint"_i18n, 64);
            return true;
        });
        searchRow->addGestureRecognizer(new brls::TapGestureRecognizer(searchRow));
        resultsBox->addView(searchRow);
    }

    if (this->lastPage.records.empty())
    {
        if (emptyLabel)
        {
            emptyLabel->setText("mods/no_results"_i18n);
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        }
        return;
    }

    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);

    for (const auto& record : this->lastPage.records)
    {
        core::ModRecord rec = record;

        auto* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(48.0f);
        row->setFocusable(true);
        row->setMarginBottom(8.0f);
        row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        row->setCornerRadius(12.0f);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23)); // surface_1

        auto* nameLabel = new brls::Label();
        nameLabel->setText(rec.name);
        nameLabel->setFontSize(16.0f);
        nameLabel->setGrow(1.0f);
        row->addView(nameLabel);

        auto* likesLabel = new brls::Label();
        likesLabel->setText(std::to_string(rec.likes) + " " + "mods/likes"_i18n);
        likesLabel->setFontSize(14.0f);
        likesLabel->setMarginLeft(12.0f);
        row->addView(likesLabel);

        bool manual = (this->gameId == 0); // global free-text search => confirm target
        row->registerClickAction([this, rec, manual](brls::View*) {
            brls::Application::pushActivity(
                new ModDetailActivity(this->title, rec.id, this->http, manual));
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        resultsBox->addView(row);
    }

    // "Load more" row when the page is not the last. NOTE (draft): the search
    // replaces the list with the next page rather than appending — acceptable for
    // now; a future refinement would accumulate records across pages.
    if (!this->lastPage.is_complete)
    {
        auto* moreRow = new brls::Box(brls::Axis::ROW);
        moreRow->setHeight(48.0f);
        moreRow->setFocusable(true);
        moreRow->setMarginTop(8.0f);
        moreRow->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        moreRow->setCornerRadius(12.0f);
        moreRow->setJustifyContent(brls::JustifyContent::CENTER);
        moreRow->setAlignItems(brls::AlignItems::CENTER);
        moreRow->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2

        auto* moreLabel = new brls::Label();
        moreLabel->setText("mods/load_more"_i18n);
        moreLabel->setFontSize(16.0f);
        moreRow->addView(moreLabel);

        moreRow->registerClickAction([this](brls::View*) {
            this->page++;
            // Deferred: this rebuild runs from inside a view-click handler, so it
            // MUST be wrapped in brls::sync to avoid destroying the row mid-event
            // (use-after-free). M1 lesson. Game mode pages the Subfeed; otherwise
            // the global free-text search.
            brls::sync([this]() {
                if (this->gameId != 0)
                    this->runGameSearch(this->query);
                else
                    this->runGlobalSearch();
            });
            return true;
        });
        moreRow->addGestureRecognizer(new brls::TapGestureRecognizer(moreRow));
        resultsBox->addView(moreRow);
    }
}

} // namespace thomaz
