#include "app/boot_activity.hpp"
#include "app/home_activity.hpp"
#include "app/auth_activity.hpp"
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>

namespace thomaz {

BootActivity::BootActivity(ITitleService* titleService,
                           IHttpClient* http,
                           ISaveService* saveService,
                           IAuthClient* feed,
                           ICloudSaveClient* cloudSaves)
    : titleService(titleService)
    , http(http)
    , saveService(saveService)
    , feed(feed)
    , cloudSaves(cloudSaves)
{}

void BootActivity::goHome()
{
    // Pop BootActivity (no transition) then push HomeActivity so BootActivity
    // does not linger under HomeActivity in the navigation stack.
    ITitleService*    ts  = this->titleService;
    IHttpClient*      h   = this->http;
    ISaveService*     ss  = this->saveService;
    IAuthClient*      f   = this->feed;
    ICloudSaveClient* cs  = this->cloudSaves;
    brls::Application::popActivity(
        brls::TransitionAnimation::NONE,
        [ts, h, ss, f, cs]() {
            brls::Application::pushActivity(
                new thomaz::HomeActivity(ts, h, ss, f, cs));
        });
}

void BootActivity::onContentAvailable()
{
    auto* loginBtn = this->getView("loginBtn");
    if (!loginBtn) {
        brls::Logger::error("boot.xml: loginBtn missing");
        return;
    }
    auto* guestBtn = this->getView("guestBtn");
    if (!guestBtn) {
        brls::Logger::error("boot.xml: guestBtn missing");
        return;
    }

    // loginBtn: push AuthActivity; on successful auth the callback pops
    // BootActivity and pushes HomeActivity via goHome().
    loginBtn->registerClickAction([this](brls::View*) {
        IAuthClient* f = this->feed;
        brls::Application::pushActivity(
            new thomaz::AuthActivity(f, [this]() { this->goHome(); }));
        return true;
    });
    loginBtn->addGestureRecognizer(new brls::TapGestureRecognizer(loginBtn));

    // guestBtn: go directly to home without authentication.
    guestBtn->registerClickAction([this](brls::View*) {
        this->goHome();
        return true;
    });
    guestBtn->addGestureRecognizer(new brls::TapGestureRecognizer(guestBtn));

    // Default focus on the login button.
    brls::Application::giveFocus(loginBtn);
}

} // namespace thomaz
