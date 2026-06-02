/*
    thomaz — home activity (bento hub).
    Shows a Trapaças card (navigates to game list) and three disabled "Em breve" cards.
*/

#pragma once

#include <borealis.hpp>
#include "platform/title.hpp"

namespace thomaz {

class HomeActivity : public brls::Activity
{
  public:
    HomeActivity(ITitleService* titleService);

    CONTENT_FROM_XML_RES("activity/home.xml");

    void onContentAvailable() override;

  private:
    ITitleService* titleService;
};

} // namespace thomaz
