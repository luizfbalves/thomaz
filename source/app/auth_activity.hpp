#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <borealis.hpp>
#include "platform/auth_client.hpp"

namespace thomaz {

// Tela de login/cadastro. Recebe o IAuthClient e um callback chamado ao
// autenticar com sucesso (a sessão já foi persistida) para a tela que pediu
// login retomar sua ação.
class AuthActivity : public brls::Activity {
  public:
    AuthActivity(IAuthClient* client, std::function<void()> onAuthed);
    ~AuthActivity() override;

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
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
