/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/game_list_activity.hpp"
#include "app/settings_activity.hpp"
#include "app/save_manager_activity.hpp"

#include <borealis.hpp>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService)
    : titleService(titleService), http(http), saveService(saveService)
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
        settings->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new SettingsActivity(this->http));
            return true;
        });
        settings->addGestureRecognizer(new brls::TapGestureRecognizer(settings));
    }

    if (brls::View* saves = this->getView("savesCard")) {
        saves->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(
                new SaveManagerActivity(this->titleService, this->saveService));
            return true;
        });
        saves->addGestureRecognizer(new brls::TapGestureRecognizer(saves));
    }
}

} // namespace thomaz
