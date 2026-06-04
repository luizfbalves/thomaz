#pragma once
#include "platform/auth_client.hpp"

namespace thomaz {

// In-memory stub for running/testing the app on desktop: accepts any
// register/login and returns a deterministic token (no network, no randomness).
class FakeAuthClient : public IAuthClient {
  public:
    AuthResult registerUser(const std::string& user, const std::string& pass) override;
    AuthResult login(const std::string& user, const std::string& pass) override;
};

} // namespace thomaz
