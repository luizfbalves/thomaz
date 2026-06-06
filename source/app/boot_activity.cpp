#include "app/boot_activity.hpp"
#include "app/home_activity.hpp"
#include "app/auth_activity.hpp"
#include "app/version.hpp"
#include <borealis.hpp>

namespace thomaz {

BootActivity::BootActivity(IAuthClient* feed, HomeActivity* home)
    : feed(feed), home(home) {}

void BootActivity::reveal()
{
    // A HomeActivity é a atividade raiz, logo abaixo de nós. Basta nos remover
    // para revelá-la. popActivity se recusa a popar a raiz (application.cpp),
    // por isso a Home precisa estar na base — ver main.cpp.
    brls::Application::popActivity(brls::TransitionAnimation::FADE);
}

void BootActivity::onLoggedIn()
{
    // A sessão já foi persistida pela AuthActivity. A Home foi construída antes
    // do login, então seu header ainda não mostra o usuário — atualiza agora,
    // depois revela a Home.
    if (this->home) this->home->refreshHeaderUsername();
    this->reveal();
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

    // Entrar: abre a AuthActivity. No sucesso ela se remove e dispara o
    // callback, que atualiza o header e revela a Home.
    loginBtn->registerClickAction([this](brls::View*) {
        brls::Application::pushActivity(
            new thomaz::AuthActivity(this->feed, [this]() { this->onLoggedIn(); }));
        return true;
    });
    loginBtn->addGestureRecognizer(new brls::TapGestureRecognizer(loginBtn));

    // Continuar sem login: apenas revela a Home (sem sessão).
    guestBtn->registerClickAction([this](brls::View*) {
        this->reveal();
        return true;
    });
    guestBtn->addGestureRecognizer(new brls::TapGestureRecognizer(guestBtn));

    // Versão atual do app, abaixo do texto de features.
    if (auto* ver = (brls::Label*)this->getView("bootVersion"))
        ver->setText(std::string("v") + THOMAZ_VERSION);

    // Foco inicial no botão de login.
    brls::Application::giveFocus(loginBtn);
}

} // namespace thomaz
