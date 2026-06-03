/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/game_list_activity.hpp"
#include "app/settings_activity.hpp"
#include "app/save_manager_activity.hpp"
#include "app/feed_activity.hpp"

#include <borealis.hpp>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                           IFeedClient* feed, IAlbumSource* album, ICloudSaveClient* cloudSaves)
    : titleService(titleService), http(http), saveService(saveService), feed(feed), album(album),
      cloudSaves(cloudSaves)
{
}

void HomeActivity::onContentAvailable()
{
    // Feed hero → community feed.
    if (brls::View* feedCard = this->getView("feedCard"))
    {
        feedCard->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(
                new FeedActivity(this->feed, this->album, this->titleService));
            return true;
        });
        feedCard->addGestureRecognizer(new brls::TapGestureRecognizer(feedCard));
    }

    // Cheats card → game list (this is the old "trapacasCard" entry; the home
    // was redesigned and the cheats tile is now "cheatsCard").
    if (brls::View* cheats = this->getView("cheatsCard"))
    {
        cheats->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new GameListActivity(this->titleService, this->http));
            return true;
        });
        // Respond to touch (Switch) and mouse (desktop), not just the A button.
        cheats->addGestureRecognizer(new brls::TapGestureRecognizer(cheats));
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
