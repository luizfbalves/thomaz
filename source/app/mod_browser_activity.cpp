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

    // Open the on-screen keyboard for the search query. The callback runs on the
    // UI thread; it is not a view-click handler, so runSearch() can be called
    // directly (no deferred brls::sync needed here).
    brls::Application::getPlatform()->getImeManager()->openForText(
        [this](std::string q) {
            if (q.empty())
            {
                if (auto* emptyLabel = (brls::Label*)this->getView("emptyLabel"))
                {
                    emptyLabel->setText("mods/no_results"_i18n);
                    emptyLabel->setVisibility(brls::Visibility::VISIBLE);
                }
                return;
            }
            this->query = q;
            this->page  = 1;
            this->runSearch();
        },
        "mods/search"_i18n, "mods/search_hint"_i18n, 64);
}

void ModBrowserActivity::runSearch()
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

        row->registerClickAction([this, rec](brls::View*) {
            brls::Application::pushActivity(new ModDetailActivity(this->title, rec.id, this->http));
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        resultsBox->addView(row);
    }

    // "Load more" row when the page is not the last. NOTE (draft): runSearch()
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
            // (use-after-free). M1 lesson.
            brls::sync([this]() { this->runSearch(); });
            return true;
        });
        moreRow->addGestureRecognizer(new brls::TapGestureRecognizer(moreRow));
        resultsBox->addView(moreRow);
    }
}

} // namespace thomaz
