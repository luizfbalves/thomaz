#pragma once
#include <string>
#include "core/feed/session_codec.hpp"

namespace thomaz {

// AuthResult lives in core/feed/session_codec.hpp (so pure core code can
// produce it). Re-expose it unqualified here for the existing call sites
// (AuthActivity and friends).
using feed::AuthResult;

// Auth-only network contract. FakeAuthClient runs on the desktop; HttpAuthClient
// plugs the real API. All methods are called from a brls::async worker thread
// and must not touch the UI. Login/register results feed into Cloud Saves via
// the session stored in auth_store.
class IAuthClient {
  public:
    virtual ~IAuthClient() = default;

    virtual AuthResult registerUser(const std::string& user, const std::string& pass) = 0;
    virtual AuthResult login(const std::string& user, const std::string& pass) = 0;
};

} // namespace thomaz
