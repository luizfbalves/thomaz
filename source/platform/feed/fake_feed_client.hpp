#pragma once
#include "platform/feed/feed_client.hpp"

namespace thomaz {

// Backend em memória para rodar/testar o app no desktop: aceita qualquer
// cadastro/login e devolve um token determinístico (sem rede, sem random).
class FakeFeedClient : public IFeedClient {
  public:
    AuthResult registerUser(const std::string& user, const std::string& pass) override;
    AuthResult login(const std::string& user, const std::string& pass) override;
};

} // namespace thomaz
