#include "platform/feed/http_feed_client.hpp"
#include "core/feed/feed_json.hpp"
#include <cstdio>

namespace thomaz {

namespace {
std::string titleIdHex(std::uint64_t id) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(id));
    return std::string(buf);
}
} // namespace

HttpFeedClient::HttpFeedClient(IHttpClient* http, std::string baseUrl,
                               std::optional<feed::Session> restored,
                               std::function<void(const feed::Session&)> onSessionChanged,
                               std::function<void()> onAuthLost)
    : http(http), baseUrl(std::move(baseUrl)),
      onSessionChanged(std::move(onSessionChanged)), onAuthLost(std::move(onAuthLost))
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

bool HttpFeedClient::tryRefresh()
{
    std::string refreshTok;
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (!hasSession || session.refreshToken.empty()) return false;
        refreshTok = session.refreshToken;
    }

    HttpRequest req;
    req.method = HttpMethod::Post;
    req.url    = url("/auth/refresh");
    req.headers.push_back({ "Content-Type", "application/json" });
    req.body   = feed::build_refresh_body(refreshTok);

    HttpResponse resp = http->request(req);
    feed::RefreshResult rr = feed::parse_refresh_response(resp.body, resp.status);
    if (!rr.ok) return false;

    std::lock_guard<std::mutex> lock(sessionMutex);
    session.token        = rr.token;
    session.refreshToken = rr.refreshToken;
    if (onSessionChanged) onSessionChanged(session);
    return true;
}

HttpResponse HttpFeedClient::authedRequest(HttpRequest req)
{
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (hasSession)
            req.headers.push_back({ "Authorization", "Bearer " + session.token });
    }

    HttpResponse resp = http->request(req);
    if (resp.status != 401) return resp;

    if (tryRefresh()) {
        for (auto& h : req.headers)
            if (h.first == "Authorization") {
                std::lock_guard<std::mutex> lock(sessionMutex);
                h.second = "Bearer " + session.token;
            }
        return http->request(req);
    }

    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        hasSession = false;
        session = feed::Session{};
    }
    if (onAuthLost) onAuthLost();
    return resp;
}

feed::FeedPage HttpFeedClient::fetchFeed(const std::string& cursor)
{
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = url("/feed") + (cursor.empty() ? "" : ("?cursor=" + cursor));
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (hasSession)
            req.headers.push_back({ "Authorization", "Bearer " + session.token });
    }
    HttpResponse resp = http->request(req); // read: do not refresh on 401
    feed::FeedPage page = feed::parse_feed_page(resp.body);
    page.ok = resp.ok(); // distinguish a real failure from a successful empty feed
    return page;
}

std::vector<std::uint8_t> HttpFeedClient::fetchImage(const std::string& imageUrl)
{
    if (imageUrl.empty()) return {};
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = imageUrl; // imageUrl já é absoluta (a API devolve a URL completa)
    HttpResponse resp = http->request(req);
    if (!resp.ok()) return {};
    return std::vector<std::uint8_t>(resp.body.begin(), resp.body.end());
}

ActionResult HttpFeedClient::createPost(const std::string& /*token (vestigial)*/,
                                        const std::vector<std::uint8_t>& jpeg,
                                        const std::string& caption,
                                        std::uint64_t gameTitleId,
                                        const std::string& gameName)
{
    HttpRequest req;
    req.method = HttpMethod::Post;
    req.url    = url("/posts");
    req.fields.push_back({ "caption", caption });
    req.fields.push_back({ "gameTitleId", titleIdHex(gameTitleId) });
    req.fields.push_back({ "gameName", gameName });
    req.files.push_back(MultipartFile{ "image", "post.jpg", "image/jpeg", jpeg });

    HttpResponse resp = authedRequest(std::move(req));
    if (resp.ok()) return ActionResult{ true, "" };
    AuthResult e = feed::parse_auth_response(resp.body, resp.status);
    return ActionResult{ false, e.error };
}

ActionResult HttpFeedClient::setLike(const std::string& /*token*/,
                                     const std::string& postId, bool liked)
{
    HttpRequest req;
    req.method = HttpMethod::Put;
    req.url    = url("/posts/" + postId + "/like");
    req.headers.push_back({ "Content-Type", "application/json" });
    req.body   = feed::build_like_body(liked);

    HttpResponse resp = authedRequest(std::move(req));
    if (resp.ok()) return ActionResult{ true, "" };
    AuthResult e = feed::parse_auth_response(resp.body, resp.status);
    return ActionResult{ false, e.error };
}

std::vector<feed::Comment> HttpFeedClient::fetchComments(const std::string& postId)
{
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = url("/posts/" + postId + "/comments");
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (hasSession)
            req.headers.push_back({ "Authorization", "Bearer " + session.token });
    }
    HttpResponse resp = http->request(req);
    return feed::parse_comments(resp.body);
}

ActionResult HttpFeedClient::addComment(const std::string& /*token*/,
                                        const std::string& postId, const std::string& text)
{
    HttpRequest req;
    req.method = HttpMethod::Post;
    req.url    = url("/posts/" + postId + "/comments");
    req.headers.push_back({ "Content-Type", "application/json" });
    req.body   = feed::build_comment_body(text);

    HttpResponse resp = authedRequest(std::move(req));
    if (resp.ok()) return ActionResult{ true, "" };
    AuthResult e = feed::parse_auth_response(resp.body, resp.status);
    return ActionResult{ false, e.error };
}

} // namespace thomaz
