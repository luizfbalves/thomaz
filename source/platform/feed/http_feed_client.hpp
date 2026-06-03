#pragma once
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include "platform/feed/feed_client.hpp"
#include "platform/http_client.hpp"
#include "core/feed/feed_types.hpp"

namespace thomaz {

// Real IFeedClient backed by the thomaz-api over HTTP: login/register only.
// Owns the session and persists it via onSessionChanged so Cloud Saves can read
// the token from auth_store. Methods run on a brls::async worker thread; a mutex
// guards the session.
class HttpFeedClient : public IFeedClient {
  public:
    HttpFeedClient(IHttpClient* http,
                   std::string baseUrl,
                   std::optional<feed::Session> restored,
                   std::function<void(const feed::Session&)> onSessionChanged);

    AuthResult registerUser(const std::string& user, const std::string& pass) override;
    AuthResult login(const std::string& user, const std::string& pass) override;

  private:
    AuthResult doAuth(const std::string& path,
                      const std::string& user, const std::string& pass);
    std::string url(const std::string& path) const { return baseUrl + path; }

    IHttpClient*  http;
    std::string   baseUrl;             // no trailing slash
    feed::Session session;
    bool          hasSession = false;
    std::mutex    sessionMutex;
    std::function<void(const feed::Session&)> onSessionChanged;
};

} // namespace thomaz
