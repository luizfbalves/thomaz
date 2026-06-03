#pragma once
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include "platform/feed/feed_client.hpp"
#include "platform/http_client.hpp"
#include "core/feed/feed_types.hpp"

namespace thomaz {

// Real IFeedClient backed by the thomaz-api over HTTP. Owns the session
// (the access token rotates on refresh, so the caller cannot own it). All
// methods run on a brls::async worker thread; a mutex guards the session.
class HttpFeedClient : public IFeedClient {
  public:
    HttpFeedClient(IHttpClient* http,
                   std::string baseUrl,
                   std::optional<feed::Session> restored,
                   std::function<void(const feed::Session&)> onSessionChanged,
                   std::function<void()> onAuthLost);

    AuthResult registerUser(const std::string& user, const std::string& pass) override;
    AuthResult login(const std::string& user, const std::string& pass) override;
    feed::FeedPage fetchFeed(const std::string& cursor) override;
    ActionResult createPost(const std::string& token,
                            const std::vector<std::uint8_t>& jpeg,
                            const std::string& caption,
                            std::uint64_t gameTitleId,
                            const std::string& gameName) override;
    ActionResult setLike(const std::string& token,
                         const std::string& postId, bool liked) override;
    std::vector<feed::Comment> fetchComments(const std::string& postId) override;
    ActionResult addComment(const std::string& token,
                            const std::string& postId, const std::string& text) override;

  private:
    AuthResult doAuth(const std::string& path,
                      const std::string& user, const std::string& pass);
    HttpResponse authedRequest(HttpRequest req); // injects Bearer, refreshes once on 401
    bool tryRefresh();                            // POST /auth/refresh; rotates + persists
    std::string url(const std::string& path) const { return baseUrl + path; }

    IHttpClient*  http;
    std::string   baseUrl;             // no trailing slash
    feed::Session session;
    bool          hasSession = false;
    std::mutex    sessionMutex;
    std::function<void(const feed::Session&)> onSessionChanged;
    std::function<void()> onAuthLost;
};

} // namespace thomaz
