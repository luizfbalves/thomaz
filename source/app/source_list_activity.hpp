#pragma once

#include <string>
#include <vector>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "core/games/source_link.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Source list entry point (empty by default — SRC-04).
class SourceListActivity : public ThomazActivity {
  public:
    SourceListActivity(ITitleService* titleService, IHttpClient* http);

    CONTENT_FROM_XML_RES("activity/source_list.xml");
    void onContentAvailable() override;

  private:
    void reload();
    void populate();
    void beginAddSource();
    void promptAuthForUrl(const std::string& url);
    void finishAddSource(const std::string& url, thomaz::core::SourceAuthType auth,
                         const std::string& secret);
    void doSync();
    void confirmRemove(std::size_t index);
    void openSource(const thomaz::core::SourceConfig& cfg);
    void setAddBusy(bool on);
    void setSyncBusy(bool on);
    void updateSyncAppearance(bool synced);
    std::string rowLabel(const thomaz::core::SourceConfig& cfg) const;

    ITitleService*                         titleService;
    IHttpClient*                         http;
    std::vector<thomaz::core::SourceConfig> sources;
    bool                                 addBusy  = false;
    bool                                 syncBusy = false;
    bool                                 syncOk   = false;
};

} // namespace thomaz
