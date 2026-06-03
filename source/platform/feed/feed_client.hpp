#pragma once
#include <string>
#include "core/feed/feed_types.hpp"

namespace thomaz {

// AuthResult lives in core/feed/feed_types.hpp (so pure core code can produce
// it). Re-expose it unqualified here for the existing call sites (AuthActivity).
using feed::AuthResult;

// Contrato de rede de autenticação. FakeFeedClient roda no desktop; HttpFeedClient
// pluga a API real. Todos os métodos são chamados de um worker thread
// (brls::async) e não tocam na UI. Login/cadastro alimentam o Cloud Saves.
class IFeedClient {
  public:
    virtual ~IFeedClient() = default;

    virtual AuthResult registerUser(const std::string& user, const std::string& pass) = 0;
    virtual AuthResult login(const std::string& user, const std::string& pass) = 0;
};

} // namespace thomaz
