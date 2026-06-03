/*
    thomaz — home activity (bento hub).
    Tiles: Cheats (hero → game list), Settings, Save Manager (→ save backup),
    and Mods. The auth client (IFeedClient) is carried through to Save Manager,
    which needs login for Cloud Saves.
*/

#pragma once

#include <borealis.hpp>
#include "platform/http_client.hpp"
#include "platform/title.hpp"
#include "platform/save_service.hpp"
#include "platform/feed/feed_client.hpp"
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

class HomeActivity : public brls::Activity
{
  public:
    HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                 IFeedClient* feed, ICloudSaveClient* cloudSaves);

    CONTENT_FROM_XML_RES("activity/home.xml");

    void onContentAvailable() override;

  private:
    ITitleService* titleService;
    IHttpClient* http;
    ISaveService* saveService;
    IFeedClient* feed;
    ICloudSaveClient* cloudSaves;
};

} // namespace thomaz
