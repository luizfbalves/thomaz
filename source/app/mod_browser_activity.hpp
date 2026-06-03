/*
    thomaz — mod browser activity.
    Free-text GameBanana mod search for one installed game. Opens the keyboard,
    lists matching mods as tappable rows, and pushes the mod detail screen on tap.
*/

#pragma once

#include <atomic>
#include <cstdint>
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
    // Called on the UI thread once resolve_game + (optional) first mod listing
    // finish. Decides between game-mode (Subfeed) and global free-text fallback.
    void onResolved(const core::GameResolve& g, const core::BrowseResult& mods);
    // Run a free-text GLOBAL mod search for `query`/`page` (off-thread).
    void runGlobalSearch();
    // Run an in-game (Subfeed) search for `query` within the resolved gameId.
    void runGameSearch(const std::string& query);
    // Build the result rows on the UI thread from a browse result.
    void populate(const core::BrowseResult& result);

    InstalledTitle title;
    IHttpClient* http;
    std::string query;
    int page = 1;
    core::SearchPage lastPage;
    // 0 = unresolved => global free-text mode; nonzero = resolved game => Subfeed.
    std::uint64_t gameId = 0;

    // Set false in the destructor so an in-flight async UI callback bails.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
