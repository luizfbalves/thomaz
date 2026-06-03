/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/app_header.hpp"
#include "app/game_list_activity.hpp"
#include "app/settings_activity.hpp"
#include "app/save_manager_activity.hpp"

#include <borealis.hpp>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                           IFeedClient* feed, ICloudSaveClient* cloudSaves)
    : titleService(titleService), http(http), saveService(saveService), feed(feed),
      cloudSaves(cloudSaves)
{
}

void HomeActivity::onContentAvailable()
{
    install_header_username(this);

    // Cheats hero → game list.
    if (brls::View* cheats = this->getView("cheatsCard"))
    {
        cheats->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new GameListActivity(this->titleService, this->http));
            return true;
        });
        // Respond to touch (Switch) and mouse (desktop), not just the A button.
        cheats->addGestureRecognizer(new brls::TapGestureRecognizer(cheats));
    }

    // Mods card → game list in Mods mode.
    if (brls::View* mods = this->getView("modsCard"))
    {
        mods->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(
                new GameListActivity(this->titleService, this->http, GameListActivity::Target::Mods));
            return true;
        });
        mods->addGestureRecognizer(new brls::TapGestureRecognizer(mods));
    }

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
                new SaveManagerActivity(this->titleService, this->saveService,
                                        this->cloudSaves, this->feed));
            return true;
        });
        saves->addGestureRecognizer(new brls::TapGestureRecognizer(saves));
    }
}

} // namespace thomaz
