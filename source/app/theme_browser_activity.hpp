#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "core/themes/themezer_browse.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Browses Themezer as a grid: a Packs/Themes toggle, free-text search, a section
// filter (Themes mode only), and a load-more row that shows the next page.
class ThemeBrowserActivity : public ThomazActivity {
  public:
    explicit ThemeBrowserActivity(IHttpClient* http);

    CONTENT_FROM_XML_RES("activity/theme_browser.xml");
    void onContentAvailable() override;

  private:
    void reload();                 // query page 1 with current mode/query/target
    void runQuery(int page);
    void populate(const thomaz::core::BrowsePage& page);
    void loadThumb(const std::string& url, brls::Image* into);
    void openSearch();
    void updateTabSelection();     // repaint the Packs/Themes badges to match packsMode

    IHttpClient* http;
    bool         packsMode = true; // start on Packs
    std::string  query;
    std::string  target;           // "" = all (Themes mode only)
    int          page = 1;
    bool         isComplete = true;

    // Bumped every time populate() rebuilds the grid (which destroys the old
    // cards). In-flight loadThumb continuations capture the generation at
    // dispatch and skip touching their (now-freed) Image if it changed —
    // prevents the use-after-free crash when sections are switched rapidly.
    std::shared_ptr<std::atomic<std::uint64_t>> listGen =
        std::make_shared<std::atomic<std::uint64_t>>(0);
};

} // namespace thomaz
