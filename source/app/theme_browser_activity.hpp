#pragma once
#include <atomic>
#include <memory>
#include <string>

#include <borealis.hpp>

#include "core/themes/themezer_browse.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Browses Themezer as a grid: a Packs/Themes toggle, free-text search, a section
// filter (Themes mode only), and a load-more row that shows the next page.
class ThemeBrowserActivity : public brls::Activity {
  public:
    explicit ThemeBrowserActivity(IHttpClient* http);
    ~ThemeBrowserActivity() override;

    CONTENT_FROM_XML_RES("activity/theme_browser.xml");
    void onContentAvailable() override;

  private:
    void reload();                 // query page 1 with current mode/query/target
    void runQuery(int page);
    void populate(const thomaz::core::BrowsePage& page);
    void loadThumb(const std::string& url, brls::Image* into);
    void openSearch();
    void cyclePart();              // advance the section filter (Themes mode)

    IHttpClient* http;
    bool         packsMode = true; // start on Packs
    std::string  query;
    std::string  target;           // "" = all (Themes mode only)
    int          page = 1;
    bool         isComplete = true;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
