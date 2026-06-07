#pragma once

#include <memory>
#include <string>

#include <borealis.hpp>

#include "app/thomaz_activity.hpp"
#include "core/games/catalog.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Read-only detail for one grouped base title: hero cover + Base/Update/DLC rows.
class CatalogDetailActivity : public ThomazActivity {
  public:
    CatalogDetailActivity(IHttpClient* http, thomaz::core::GroupedTitle title,
                          ITitleService* titleService);

    CONTENT_FROM_XML_RES("activity/catalog_detail.xml");
    void onContentAvailable() override;

  private:
    void onHeroReady();
    void buildRows();

    thomaz::core::GroupedTitle grouped;
    IHttpClient*                 http;
    ITitleService*               titleService;
};

} // namespace thomaz
