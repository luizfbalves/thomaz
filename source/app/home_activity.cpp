/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/game_list_activity.hpp"
#include "app/settings_activity.hpp"

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

    // Settings card.
    if (brls::View* settings = this->getView("settingsCard"))
    {
        settings->registerClickAction([](brls::View*) {
            brls::Application::pushActivity(new SettingsActivity());
            return true;
        });
        settings->addGestureRecognizer(new brls::TapGestureRecognizer(settings));
    }
}

} // namespace thomaz
