#include "app/auth_activity.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

AuthActivity::AuthActivity(IFeedClient* client, std::function<void()> onAuthed)
    : client(client), onAuthed(std::move(onAuthed)) {}

AuthActivity::~AuthActivity() { *this->alive = false; }

void AuthActivity::onContentAvailable()
{
    auto* userCell = (brls::InputCell*)this->getView("usernameCell");
    auto* passCell = (brls::InputCell*)this->getView("passwordCell");

    userCell->init("thomaz/auth/username"_i18n, "", [this](std::string v){ this->username = v; },
                   "thomaz/auth/username"_i18n, "", 32);
    passCell->init("thomaz/auth/password"_i18n, "", [this](std::string v){ this->password = v; },
                   "thomaz/auth/password"_i18n, "", 64);

    auto* tabsRow = (brls::Box*)this->getView("tabsRow");
    auto makeTab = [this](const std::string& text, bool regMode) {
        auto* tab = new brls::Box(brls::Axis::ROW);
        tab->setFocusable(true);
        tab->setHeight(40.0f);
        tab->setGrow(1.0f);
        tab->setCornerRadius(10.0f);
        tab->setJustifyContent(brls::JustifyContent::CENTER);
        tab->setAlignItems(brls::AlignItems::CENTER);
        tab->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
        auto* lbl = new brls::Label();
        lbl->setText(text);
        lbl->setFontSize(15.0f);
        tab->addView(lbl);
        tab->registerClickAction([this, regMode](brls::View*) {
            this->registerMode = regMode;
            this->refreshMode();
            return true;
        });
        tab->addGestureRecognizer(new brls::TapGestureRecognizer(tab));
        return tab;
    };
    tabsRow->addView(makeTab("thomaz/auth/login_tab"_i18n, false));
    auto* spacer = new brls::Box(); spacer->setWidth(10.0f); tabsRow->addView(spacer);
    tabsRow->addView(makeTab("thomaz/auth/register_tab"_i18n, true));

    auto* submit = this->getView("submitBtn");
    submit->registerClickAction([this](brls::View*) { this->submit(); return true; });
    submit->addGestureRecognizer(new brls::TapGestureRecognizer(submit));

    this->refreshMode();
}

void AuthActivity::refreshMode()
{
    auto* submitLabel = (brls::Label*)this->getView("submitLabel");
    submitLabel->setText(this->registerMode ? "thomaz/auth/submit_register"_i18n
                                            : "thomaz/auth/submit_login"_i18n);
}

void AuthActivity::submit()
{
    if (this->busy) return;
    auto* status = (brls::Label*)this->getView("authStatus");

    if (this->username.empty() || this->password.empty()) {
        status->setText("thomaz/auth/err_empty"_i18n);
        return;
    }

    this->busy = true;
    status->setText("…");

    IFeedClient* c = this->client;
    auto alive     = this->alive;
    std::string u = this->username, p = this->password;
    bool reg = this->registerMode;

    brls::async([this, c, alive, u, p, reg, status]() {
        AuthResult r = reg ? c->registerUser(u, p) : c->login(u, p);
        brls::sync([this, alive, r, u, status]() {
            if (!alive->load()) return;
            this->busy = false;
            if (!r.ok) {
                status->setText(r.error.empty() ? "thomaz/auth/err_failed"_i18n : r.error);
                return;
            }
            save_session(feed::Session{ r.token, r.refreshToken, u });
            auto cb = this->onAuthed;
            brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                           [cb]() { if (cb) cb(); });
        });
    });
}

} // namespace thomaz
