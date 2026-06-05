#include "app/auth_activity.hpp"
#include <cctype>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include "platform/feed/auth_store.hpp"

using namespace brls::literals;

namespace thomaz {

AuthActivity::AuthActivity(IAuthClient* client, std::function<void()> onAuthed)
    : client(client), onAuthed(std::move(onAuthed)) {}

void AuthActivity::onContentAvailable()
{
    auto* userCell = dynamic_cast<brls::InputCell*>(this->getView("usernameCell"));
    auto* passCell = dynamic_cast<brls::InputCell*>(this->getView("passwordCell"));
    if (!userCell || !passCell) {
        brls::Logger::error("auth.xml: username/password cell missing or wrong type");
        return;
    }

    userCell->init("thomaz/auth/username"_i18n, "", [this](std::string v){ this->username = v; },
                   "thomaz/auth/username"_i18n, "", 32);
    passCell->init("thomaz/auth/password"_i18n, "", [this](std::string v){ this->password = v; },
                   "thomaz/auth/password"_i18n, "", 64);

    auto* tabsRow = dynamic_cast<brls::Box*>(this->getView("tabsRow"));
    if (!tabsRow) {
        brls::Logger::error("auth.xml: tabsRow missing or wrong type");
        return;
    }
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
    if (!submit) {
        brls::Logger::error("auth.xml: submitBtn missing");
        return;
    }
    submit->registerClickAction([this](brls::View*) { this->submit(); return true; });
    submit->addGestureRecognizer(new brls::TapGestureRecognizer(submit));

    this->refreshMode();
}

void AuthActivity::refreshMode()
{
    auto* submitLabel = dynamic_cast<brls::Label*>(this->getView("submitLabel"));
    if (!submitLabel) {
        brls::Logger::error("auth.xml: submitLabel missing or wrong type");
        return;
    }
    submitLabel->setText(this->registerMode ? "thomaz/auth/submit_register"_i18n
                                            : "thomaz/auth/submit_login"_i18n);
}

void AuthActivity::submit()
{
    if (this->busy) return;
    auto* status = dynamic_cast<brls::Label*>(this->getView("authStatus"));
    if (!status) {
        brls::Logger::error("auth.xml: authStatus missing or wrong type");
        return;
    }

    if (this->username.empty() || this->password.empty()) {
        status->setText("thomaz/auth/err_empty"_i18n);
        return;
    }

    // Ao criar conta: exigir nome alfanumérico (3–32). A unicidade é checada na
    // API (409 → "nome já existe") antes de criar; aqui só barramos formato.
    if (this->registerMode) {
        const std::string& u = this->username;
        bool validName = u.size() >= 3 && u.size() <= 32;
        for (char ch : u)
            if (!std::isalnum(static_cast<unsigned char>(ch))) validName = false;
        if (!validName) {
            status->setText("thomaz/auth/err_username"_i18n);
            return;
        }
    }

    this->busy = true;
    status->setText("…");

    IAuthClient* c = this->client;
    std::string u = this->username, p = this->password;
    bool reg = this->registerMode;

    auto result = std::make_shared<AuthResult>();
    this->runAsync(
        [c, u, p, reg, result]() {
            *result = reg ? c->registerUser(u, p) : c->login(u, p);
        },
        [this, result, u, status]() {
            this->busy = false;
            if (!result->ok) {
                status->setText(result->error.empty() ? "thomaz/auth/err_failed"_i18n : result->error);
                return;
            }
            auto cb = this->onAuthed;
            brls::Application::popActivity(brls::TransitionAnimation::NONE,
                                           [cb]() { if (cb) cb(); });
        });
}

} // namespace thomaz
