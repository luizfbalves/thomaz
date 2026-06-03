/*
    thomaz — home activity (bento hub).
    Shows a Trapaças card (navigates to game list) and three disabled "Em breve" cards.
*/

#pragma once

#include <borealis.hpp>
#include "platform/http_client.hpp"
#include "platform/title.hpp"
#include "platform/save_service.hpp"

namespace thomaz {

class HomeActivity : public brls::Activity
{
  public:
    HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService);

    CONTENT_FROM_XML_RES("activity/home.xml");

    void onContentAvailable() override;

  private:
    ITitleService* titleService;
    IHttpClient* http;
    ISaveService* saveService;
};

} // namespace thomaz
