#pragma once
#include <functional>
#include <borealis.hpp>
#include "app/thomaz_activity.hpp"
#include "platform/auth_client.hpp"

namespace thomaz {

class HomeActivity; // forward decl — Boot reveals the Home pushed underneath it

// Tela de boas-vindas (boot) mostrada SOBRE a HomeActivity quando não há sessão
// salva: ícone à esquerda, "Entrar" ou "Continuar sem login" à direita.
// Ela não cria a Home (a Home já está na base da pilha, empurrada pelo main);
// apenas se remove para revelá-la — atualizando o header após um login.
class BootActivity : public ThomazActivity {
  public:
    BootActivity(IAuthClient* feed, HomeActivity* home);

    CONTENT_FROM_XML_RES("activity/boot.xml");
    void onContentAvailable() override;

  private:
    void reveal();     // pop self → revela a HomeActivity por baixo
    void onLoggedIn(); // atualiza o header da Home, depois revela

    IAuthClient*  feed;
    HomeActivity* home;
};

} // namespace thomaz
