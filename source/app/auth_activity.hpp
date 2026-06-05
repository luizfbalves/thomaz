#pragma once
#include <functional>
#include <memory>
#include <borealis.hpp>
#include "app/thomaz_activity.hpp"
#include "platform/auth_client.hpp"

namespace thomaz {

// Tela de login/cadastro. Recebe o IAuthClient e um callback chamado ao
// autenticar com sucesso (a sessão já foi persistida) para a tela que pediu
// login retomar sua ação.
class AuthActivity : public ThomazActivity {
  public:
    AuthActivity(IAuthClient* client, std::function<void()> onAuthed);

    CONTENT_FROM_XML_RES("activity/auth.xml");
    void onContentAvailable() override;

  private:
    void refreshMode();
    void submit();

    IAuthClient* client;
    std::function<void()> onAuthed;
    bool registerMode = false;
    bool busy = false;
    std::string username, password;
};

} // namespace thomaz
