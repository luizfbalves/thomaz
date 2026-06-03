#include "doctest.h"
#include <map>
#include <vector>
#include "platform/feed/http_feed_client.hpp"
#include "platform/http_client.hpp"

using namespace thomaz;

namespace {

std::string key(HttpMethod m, const std::string& url) {
    const char* mn = m == HttpMethod::Get ? "GET" : m == HttpMethod::Post ? "POST"
                   : m == HttpMethod::Put ? "PUT" : "DELETE";
    return std::string(mn) + " " + url;
}

// Scripted IHttpClient: returns canned responses keyed by "METHOD url" and
// records every request it received.
class FakeHttpClient : public IHttpClient {
  public:
    std::map<std::string, std::vector<HttpResponse>> scripted;
    std::vector<HttpRequest> received;

    void push(HttpMethod m, const std::string& url, long status, std::string body) {
        scripted[key(m, url)].push_back(HttpResponse{status, std::move(body)});
    }

    HttpResponse request(const HttpRequest& req) override {
        received.push_back(req);
        auto it = scripted.find(key(req.method, req.url));
        if (it == scripted.end() || it->second.empty())
            return HttpResponse{404, R"({"ok":false,"error":"not_scripted"})"};
        HttpResponse r = it->second.front();
        if (it->second.size() > 1) it->second.erase(it->second.begin());
        return r;
    }

    static std::string authHeader(const HttpRequest& r) {
        for (auto& h : r.headers)
            if (h.first == "Authorization") return h.second;
        return "";
    }
};

const std::string BASE = "http://api.test";

} // namespace

TEST_CASE("login populates session and persists token + refreshToken") {
    FakeHttpClient http;
    http.push(HttpMethod::Post, BASE + "/auth/login", 200,
              R"({"ok":true,"token":"acc","refreshToken":"ref"})");

    feed::Session saved; bool savedCalled = false;
    HttpFeedClient client(&http, BASE, std::nullopt,
        [&](const feed::Session& s){ saved = s; savedCalled = true; });

    AuthResult r = client.login("luiz", "pw");
    CHECK(r.ok);
    CHECK(savedCalled);
    CHECK(saved.token == "acc");
    CHECK(saved.refreshToken == "ref");
    CHECK(saved.username == "luiz");
}

TEST_CASE("register hits /auth/register and persists the session") {
    FakeHttpClient http;
    http.push(HttpMethod::Post, BASE + "/auth/register", 200,
              R"({"ok":true,"token":"acc","refreshToken":"ref"})");

    feed::Session saved; bool savedCalled = false;
    HttpFeedClient client(&http, BASE, std::nullopt,
        [&](const feed::Session& s){ saved = s; savedCalled = true; });

    AuthResult r = client.registerUser("luiz", "pw");
    CHECK(r.ok);
    CHECK(savedCalled);
    CHECK(saved.username == "luiz");
    CHECK(http.received.at(0).url == BASE + "/auth/register");
}

TEST_CASE("failed login does not persist a session") {
    FakeHttpClient http;
    http.push(HttpMethod::Post, BASE + "/auth/login", 401, R"({"ok":false})");

    bool savedCalled = false;
    HttpFeedClient client(&http, BASE, std::nullopt,
        [&](const feed::Session&){ savedCalled = true; });

    AuthResult r = client.login("luiz", "bad");
    CHECK_FALSE(r.ok);
    CHECK_FALSE(savedCalled);
    CHECK_FALSE(r.error.empty());
}
