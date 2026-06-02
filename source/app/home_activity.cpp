/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/game_list_activity.hpp"

#include <borealis.hpp>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http)
    : titleService(titleService), http(http)
{
}

void HomeActivity::onContentAvailable()
{
    // Register click on the Trapaças card to navigate to the game list.
    brls::View* card = this->getView("trapacasCard");
    card->setFocusable(true);
    card->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new GameListActivity(this->titleService, this->http));
        return true;
    });
    // Make the card respond to touch (Switch) and mouse (desktop), not just A.
    card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
}

} // namespace thomaz
