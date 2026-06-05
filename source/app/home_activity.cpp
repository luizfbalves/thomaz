/*
    thomaz — home activity implementation.
*/

#include "app/home_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/game_list_activity.hpp"
#include "app/settings_activity.hpp"
#include "app/save_manager_activity.hpp"
#include "app/theme_browser_activity.hpp"
#include "app/system_activity.hpp"
#include "platform/sysmod/sysmod_store.hpp"

#include <borealis.hpp>
#include <memory>

namespace thomaz {

HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                           IAuthClient* feed, ICloudSaveClient* cloudSaves)
    : titleService(titleService), http(http), saveService(saveService), feed(feed),
      cloudSaves(cloudSaves)
{
}

void HomeActivity::onContentAvailable()
{
    install_header_username(this);
    install_tls_warning_banner(this);

    // Cheats hero → game list.
    if (brls::View* cheats = this->getView("cheatsCard"))
    {
        cheats->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new GameListActivity(this->titleService, this->http),
                                            brls::TransitionAnimation::NONE);
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
                new GameListActivity(this->titleService, this->http, GameListActivity::Target::Mods),
                brls::TransitionAnimation::NONE);
            return true;
        });
        mods->addGestureRecognizer(new brls::TapGestureRecognizer(mods));
    }

    if (brls::View* themes = this->getView("themesCard")) {
        themes->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new ThemeBrowserActivity(this->http),
                                            brls::TransitionAnimation::NONE);
            return true;
        });
        themes->addGestureRecognizer(new brls::TapGestureRecognizer(themes));
    }

    // Settings card.
    if (brls::View* settings = this->getView("settingsCard"))
    {
        settings->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new SettingsActivity(this->http),
                                            brls::TransitionAnimation::NONE);
            return true;
        });
        settings->addGestureRecognizer(new brls::TapGestureRecognizer(settings));
    }

    if (brls::View* saves = this->getView("savesCard")) {
        saves->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(
                new SaveManagerActivity(this->titleService, this->saveService,
                                        this->cloudSaves, this->feed),
                brls::TransitionAnimation::NONE);
            return true;
        });
        saves->addGestureRecognizer(new brls::TapGestureRecognizer(saves));
    }

    // Sistema card → sysmodules manager.
    if (brls::View* system = this->getView("systemCard"))
    {
        system->registerClickAction([this](brls::View*) {
            auto store = std::make_shared<SysmoduleStore>();
            brls::Application::pushActivity(new SystemActivity(store),
                                            brls::TransitionAnimation::NONE);
            return true;
        });
        system->addGestureRecognizer(new brls::TapGestureRecognizer(system));
    }
}

void HomeActivity::refreshHeaderUsername()
{
    // install_header_username is a no-op when no session exists, so when this
    // runs after a boot-screen login it adds exactly the one "@username" label
    // that was missing (Home was built while still logged out).
    install_header_username(this);
}

} // namespace thomaz
