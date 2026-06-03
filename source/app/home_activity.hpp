/*
    thomaz — home activity (bento hub).
    Rail tiles: Cheats (→ game list), Settings, Save Manager (→ save backup),
    and a locked "Em breve" Mods tile. The Feed hero is wired by the feed team.
*/

#pragma once

#include <borealis.hpp>
#include "platform/http_client.hpp"
#include "platform/title.hpp"
#include "platform/save_service.hpp"
#include "platform/feed/feed_client.hpp"
#include "platform/feed/album_source.hpp"

namespace thomaz {

class HomeActivity : public brls::Activity
{
  public:
    HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                 IFeedClient* feed, IAlbumSource* album);

    CONTENT_FROM_XML_RES("activity/home.xml");

    void onContentAvailable() override;

  private:
    ITitleService* titleService;
    IHttpClient* http;
    ISaveService* saveService;
    IFeedClient* feed;
    IAlbumSource* album;
};

} // namespace thomaz
