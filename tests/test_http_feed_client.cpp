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

// Scripted IHttpClient: returns canned responses keyed by "METHOD url",
// supports a FIFO queue per key (so a URL can answer 401 then 200), and
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

    feed::Session saved; bool savedCalled = false; bool lostCalled = false;
    HttpFeedClient client(&http, BASE, std::nullopt,
        [&](const feed::Session& s){ saved = s; savedCalled = true; },
        [&](){ lostCalled = true; });

    AuthResult r = client.login("luiz", "pw");
    CHECK(r.ok);
    CHECK(savedCalled);
    CHECK(saved.token == "acc");
    CHECK(saved.refreshToken == "ref");
    CHECK(saved.username == "luiz");
    CHECK_FALSE(lostCalled);
    CHECK(FakeHttpClient::authHeader(http.received.at(0)).empty());
}

TEST_CASE("createPost sends Bearer and multipart fields") {
    FakeHttpClient http;
    http.push(HttpMethod::Post, BASE + "/posts", 200,
              R"({"ok":true,"post":{"id":"p1"}})");
    feed::Session restored{ "acc", "ref", "luiz" };
    HttpFeedClient client(&http, BASE, restored, [](const feed::Session&){}, [](){});

    std::vector<std::uint8_t> jpeg{ 0xFF, 0xD8, 0xFF };
    ActionResult r = client.createPost("ignored-token", jpeg, "cap",
                                       0x0100000000010000ULL, "Mario");
    CHECK(r.ok);
    const HttpRequest& req = http.received.at(0);
    CHECK(FakeHttpClient::authHeader(req) == "Bearer acc");
    REQUIRE(req.files.size() == 1);
    CHECK(req.files[0].field == "image");
    CHECK(req.files[0].contentType == "image/jpeg");
    bool sawTitle = false, sawName = false, sawCaption = false;
    for (auto& f : req.fields) {
        if (f.first == "gameTitleId" && f.second == "0100000000010000") sawTitle = true;
        if (f.first == "gameName" && f.second == "Mario") sawName = true;
        if (f.first == "caption" && f.second == "cap") sawCaption = true;
    }
    CHECK(sawTitle); CHECK(sawName); CHECK(sawCaption);
}

TEST_CASE("401 triggers one refresh then retries with the new token") {
    FakeHttpClient http;
    http.push(HttpMethod::Post, BASE + "/posts/p1/comments", 401, R"({"ok":false})");
    http.push(HttpMethod::Post, BASE + "/posts/p1/comments", 200, R"({"ok":true})");
    http.push(HttpMethod::Post, BASE + "/auth/refresh", 200,
              R"({"ok":true,"token":"acc2","refreshToken":"ref2"})");

    feed::Session restored{ "acc1", "ref1", "luiz" };
    feed::Session saved; bool savedCalled = false;
    HttpFeedClient client(&http, BASE, restored,
        [&](const feed::Session& s){ saved = s; savedCalled = true; }, [](){});

    ActionResult r = client.addComment("ignored", "p1", "gg");
    CHECK(r.ok);
    CHECK(savedCalled);
    CHECK(saved.token == "acc2");
    CHECK(saved.refreshToken == "ref2");
    REQUIRE(http.received.size() == 3);
    CHECK(FakeHttpClient::authHeader(http.received[0]) == "Bearer acc1");
    CHECK(http.received[1].url == BASE + "/auth/refresh");
    CHECK(FakeHttpClient::authHeader(http.received[2]) == "Bearer acc2");
}

TEST_CASE("failed refresh clears session via onAuthLost, no loop") {
    FakeHttpClient http;
    http.push(HttpMethod::Post, BASE + "/posts/p1/comments", 401, R"({"ok":false})");
    http.push(HttpMethod::Post, BASE + "/auth/refresh", 401, R"({"ok":false})");

    feed::Session restored{ "acc1", "ref1", "luiz" };
    bool lostCalled = false;
    HttpFeedClient client(&http, BASE, restored,
        [](const feed::Session&){}, [&](){ lostCalled = true; });

    ActionResult r = client.addComment("ignored", "p1", "gg");
    CHECK_FALSE(r.ok);
    CHECK(lostCalled);
    REQUIRE(http.received.size() == 2);
}

TEST_CASE("fetchFeed without session sends no auth header") {
    FakeHttpClient http;
    http.push(HttpMethod::Get, BASE + "/feed", 200, R"({"posts":[],"hasMore":false})");
    HttpFeedClient client(&http, BASE, std::nullopt, [](const feed::Session&){}, [](){});

    client.fetchFeed("");
    CHECK(FakeHttpClient::authHeader(http.received.at(0)).empty());
}

TEST_CASE("fetchFeed with session sends Bearer; anonymous 401 does not refresh") {
    FakeHttpClient http;
    http.push(HttpMethod::Get, BASE + "/feed", 200, R"({"posts":[],"hasMore":false})");
    feed::Session restored{ "acc", "ref", "luiz" };
    HttpFeedClient client(&http, BASE, restored, [](const feed::Session&){}, [](){});

    client.fetchFeed("");
    CHECK(FakeHttpClient::authHeader(http.received.at(0)) == "Bearer acc");

    FakeHttpClient http2;
    http2.push(HttpMethod::Get, BASE + "/feed", 401, R"({"ok":false})");
    bool lost = false;
    HttpFeedClient client2(&http2, BASE, restored, [](const feed::Session&){}, [&](){ lost = true; });
    client2.fetchFeed("");
    CHECK(http2.received.size() == 1); // no refresh attempt
    CHECK_FALSE(lost);
}
