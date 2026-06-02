/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/game_list_activity.hpp"

#include <borealis.hpp>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService)
    : titleService(titleService)
{
}

void HomeActivity::onContentAvailable()
{
    // Register click on the Trapaças card to navigate to the game list.
    this->getView("trapacasCard")->setFocusable(true);
    this->getView("trapacasCard")->registerClickAction([this](brls::View* view) {
        brls::Application::pushActivity(new GameListActivity(this->titleService));
        return true;
    });
}

} // namespace thomaz
