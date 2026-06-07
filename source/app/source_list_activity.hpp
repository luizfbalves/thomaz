#pragma once

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Source list entry point (empty by default — SRC-04). Body implemented in Plan 06.
class SourceListActivity : public ThomazActivity {
  public:
    SourceListActivity(ITitleService* titleService, IHttpClient* http);

    CONTENT_FROM_XML_RES("activity/source_list.xml");
    void onContentAvailable() override;

  private:
    ITitleService* titleService;
    IHttpClient*   http;
};

} // namespace thomaz
