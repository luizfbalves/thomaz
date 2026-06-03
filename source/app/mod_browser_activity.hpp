/*
    thomaz — mod browser activity.
    Free-text GameBanana mod search for one installed game. Opens the keyboard,
    lists matching mods as tappable rows, and pushes the mod detail screen on tap.
*/

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <borealis.hpp>

#include "core/mods/mod_browse.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class ModBrowserActivity : public brls::Activity
{
  public:
    ModBrowserActivity(InstalledTitle title, IHttpClient* http);
    ~ModBrowserActivity() override;

    CONTENT_FROM_XML_RES("activity/mod_browser.xml");

    void onContentAvailable() override;

  private:
    // Run a search for the current `query`/`page` (off-thread), then populate.
    void runSearch();
    // Build the result rows on the UI thread from a browse result.
    void populate(const core::BrowseResult& result);

    InstalledTitle title;
    IHttpClient* http;
    std::string query;
    int page = 1;
    core::SearchPage lastPage;

    // Set false in the destructor so an in-flight async UI callback bails.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
