/*
    thomaz — game list activity.
    Displays installed titles from an ITitleService; tapping any title shows
    a "coming soon" toast.
*/

#pragma once

#include <borealis.hpp>
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class GameListActivity : public brls::Activity
{
  public:
    GameListActivity(ITitleService* titleService, IHttpClient* http);

    CONTENT_FROM_XML_RES("activity/game_list.xml");

    void onContentAvailable() override;

  private:
    ITitleService* titleService;
    IHttpClient* http;
};

} // namespace thomaz
