#include "platform/feed/http_feed_client.hpp"
#include "core/feed/feed_json.hpp"

namespace thomaz {

HttpFeedClient::HttpFeedClient(IHttpClient* http, std::string baseUrl,
                               std::optional<feed::Session> restored,
                               std::function<void(const feed::Session&)> onSessionChanged)
    : http(http), baseUrl(std::move(baseUrl)),
      onSessionChanged(std::move(onSessionChanged))
{
    if (restored) { session = *restored; hasSession = true; }
}

AuthResult HttpFeedClient::doAuth(const std::string& path,
                                  const std::string& user, const std::string& pass)
{
    HttpRequest req;
    req.method = HttpMethod::Post;
    req.url    = url(path);
    req.headers.push_back({ "Content-Type", "application/json" });
    req.body   = feed::build_credentials_body(user, pass);

    HttpResponse resp = http->request(req);
    AuthResult r = feed::parse_auth_response(resp.body, resp.status);
    if (r.ok) {
        std::lock_guard<std::mutex> lock(sessionMutex);
        session = feed::Session{ r.token, r.refreshToken, user };
        hasSession = true;
        if (onSessionChanged) onSessionChanged(session);
    }
    return r;
}

AuthResult HttpFeedClient::registerUser(const std::string& user, const std::string& pass)
{
    return doAuth("/auth/register", user, pass);
}

AuthResult HttpFeedClient::login(const std::string& user, const std::string& pass)
{
    return doAuth("/auth/login", user, pass);
}

} // namespace thomaz
