# Real Auth Flow + HttpFeedClient Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the in-memory `FakeFeedClient` with a real `HttpFeedClient` that talks to the `thomaz-api` over HTTP, with persistent sessions and transparent access-token refresh.

**Architecture:** Extend the GET-only `IHttpClient` into a general request API (headers/body/multipart) implemented by `CurlHttpClient`. Add pure, unit-tested JSON parse/build functions in `core/feed/feed_json`. Build a stateful `HttpFeedClient` (implements the existing `IFeedClient`) that owns the session, injects the Bearer token, and refreshes once on a 401. Wire it in `main.cpp` on both platforms with an API base URL that has a compiled default plus a Settings override.

**Tech Stack:** C++17, libcurl (`curl_mime` for multipart), nlohmann/json (`lib/json`), Borealis UI, doctest (`tests/`), CMake (globs `source/**/*.cpp`).

**Design spec:** `docs/superpowers/specs/2026-06-03-real-auth-flow-design.md`

---

## Conventions used throughout

- **Git identity (MANDATORY):** every commit uses
  `git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "..."`
  with a trailing `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` line. Never use the ambient `dev@orul.tech`.
- **Do NOT push or tag.** Local commits only until the user confirms stability.
- **Unit tests:** `cd tests && make test`. The Makefile globs `tests/*.cpp`, `../source/core/*.cpp`, `../source/core/feed/*.cpp`, and `../source/platform/cheat_store.cpp`. New `core/feed/*.cpp` files are picked up automatically; `http_feed_client.cpp` must be added to `SRCS` explicitly (Task 6).
- **Desktop build (smoke):** `cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` then `make -C build_desktop -j$(nproc)`. Run with `timeout 5 ./build_desktop/thomaz` — **exit 124 = healthy** (window loop ran, SIGTERM'd), **139 = SIGSEGV**. Run without redirecting stdout to see Borealis logs (stdout is block-buffered when redirected).
- The JSON lib is included as `#include <nlohmann/json.hpp>` (header in `lib/json/...`). Confirm the include path matches existing usage (`grep -rn "nlohmann/json" source/`); reuse whatever form existing code uses.

---

## Task 1: Session model + AuthResult/ActionResult relocation + 3-line codec

Moves `AuthResult`/`ActionResult` into `core/feed/feed_types.hpp` (so the pure `feed_json` in Task 3 can populate them without depending on a platform header), adds `refreshToken` to both `Session` and `AuthResult`, and upgrades the on-disk session format to 3 lines with backward-compatible parsing of the old 2-line files.

**Files:**
- Modify: `source/core/feed/feed_types.hpp`
- Modify: `source/platform/feed/feed_client.hpp` (remove the now-relocated structs)
- Modify: `source/core/feed/session_codec.cpp`
- Test: `tests/test_session_codec.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `tests/test_session_codec.cpp`:

```cpp
// doctest's main is provided once by tests/test_main.cpp — do NOT define it here.
#include "doctest.h"
#include "core/feed/session_codec.hpp"

using namespace thomaz::feed;

TEST_CASE("session round-trips token, refreshToken, username") {
    Session s{ "acc-tok", "ref-tok", "luiz" };
    std::string text = serialize_session(s);
    auto parsed = parse_session(text);
    REQUIRE(parsed.has_value());
    CHECK(parsed->token == "acc-tok");
    CHECK(parsed->refreshToken == "ref-tok");
    CHECK(parsed->username == "luiz");
}

TEST_CASE("legacy 2-line session parses with empty refreshToken") {
    // Old format: "<token>\n<username>\n"
    auto parsed = parse_session("acc-tok\nluiz\n");
    REQUIRE(parsed.has_value());
    CHECK(parsed->token == "acc-tok");
    CHECK(parsed->refreshToken.empty());
    CHECK(parsed->username == "luiz");
}

TEST_CASE("empty/garbage session is rejected") {
    CHECK_FALSE(parse_session("").has_value());
    CHECK_FALSE(parse_session("\n\n").has_value());
}
```

> Before writing, check how the existing `tests/*.cpp` provide doctest's `main` (one file defines `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`, the rest just include the header). Match that pattern — do not define `main` twice.

- [ ] **Step 2: Run test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — compile error (`Session` has no `refreshToken` member) or the legacy/round-trip assertions fail.

- [ ] **Step 3: Move AuthResult/ActionResult to feed_types.hpp and add refreshToken**

In `source/core/feed/feed_types.hpp`, add `refreshToken` to `Session` and add the two result structs (currently defined in `feed_client.hpp`). `refreshToken` is appended **last** in `AuthResult` so `FakeFeedClient`'s positional brace-init `{ ok, token, error }` keeps compiling.

```cpp
struct Session {
    std::string token;        // short-lived access token
    std::string refreshToken; // long-lived, rotates on each /auth/refresh
    std::string username;
};

// Network call results (moved here from feed_client.hpp so pure core code can
// produce them without depending on the platform layer).
struct AuthResult {
    bool        ok = false;
    std::string token;        // access token when ok
    std::string error;        // human/i18n message when !ok
    std::string refreshToken; // refresh token when ok (appended last on purpose)
};

struct ActionResult {
    bool        ok = false;
    std::string error;
};
```

- [ ] **Step 4: Remove the relocated structs from feed_client.hpp**

In `source/platform/feed/feed_client.hpp`, delete the local `AuthResult` and `ActionResult` definitions. The file already does `#include "core/feed/feed_types.hpp"`, so the moved structs remain visible to every includer. Leave everything else unchanged.

- [ ] **Step 5: Update session_codec for 3 lines + legacy fallback**

Replace the body of `source/core/feed/session_codec.cpp`:

```cpp
#include "core/feed/session_codec.hpp"
#include <sstream>

namespace thomaz::feed {

std::string serialize_session(const Session& s)
{
    return s.token + "\n" + s.refreshToken + "\n" + s.username + "\n";
}

std::optional<Session> parse_session(const std::string& text)
{
    std::istringstream in(text);
    std::string l1, l2, l3;
    if (!std::getline(in, l1))
        return std::nullopt;
    bool hasL2 = static_cast<bool>(std::getline(in, l2));
    bool hasL3 = static_cast<bool>(std::getline(in, l3));

    auto trim = [](std::string& v) {
        const char* ws = " \t\r\n";
        auto a = v.find_first_not_of(ws);
        if (a == std::string::npos) { v.clear(); return; }
        auto b = v.find_last_not_of(ws);
        v = v.substr(a, b - a + 1);
    };

    Session s;
    if (hasL2 && hasL3) {
        // New 3-line format: token / refreshToken / username
        s.token = l1; s.refreshToken = l2; s.username = l3;
    } else if (hasL2) {
        // Legacy 2-line format: token / username (no refresh token)
        s.token = l1; s.refreshToken = ""; s.username = l2;
    } else {
        return std::nullopt;
    }
    trim(s.token); trim(s.refreshToken); trim(s.username);

    if (s.token.empty() || s.username.empty())
        return std::nullopt;
    return s;
}

} // namespace thomaz::feed
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cd tests && make test`
Expected: PASS — all three new cases pass; previously passing tests still pass.

- [ ] **Step 7: Commit**

```bash
git add source/core/feed/feed_types.hpp source/platform/feed/feed_client.hpp source/core/feed/session_codec.cpp tests/test_session_codec.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(auth): add refreshToken to Session, relocate result structs, 3-line codec

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: feed_json request-body builders

Pure functions that build the JSON request bodies. No IO. Written first because the parsers (Task 3) and client (Task 5) depend on them and they are the simplest to verify.

**Files:**
- Create: `source/core/feed/feed_json.hpp`
- Create: `source/core/feed/feed_json.cpp`
- Test: `tests/test_feed_json.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `tests/test_feed_json.cpp`:

```cpp
#include "doctest.h"
#include <nlohmann/json.hpp>
#include "core/feed/feed_json.hpp"

using namespace thomaz::feed;
using nlohmann::json;

TEST_CASE("build_credentials_body emits username and password") {
    auto j = json::parse(build_credentials_body("luiz", "s3cret"));
    CHECK(j.at("username") == "luiz");
    CHECK(j.at("password") == "s3cret");
}

TEST_CASE("build_refresh_body emits refreshToken") {
    auto j = json::parse(build_refresh_body("ref-tok"));
    CHECK(j.at("refreshToken") == "ref-tok");
}

TEST_CASE("build_like_body emits boolean liked") {
    CHECK(json::parse(build_like_body(true)).at("liked") == true);
    CHECK(json::parse(build_like_body(false)).at("liked") == false);
}

TEST_CASE("build_comment_body emits text") {
    auto j = json::parse(build_comment_body("nice run!"));
    CHECK(j.at("text") == "nice run!");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — `feed_json.hpp` not found / builder functions undefined.

- [ ] **Step 3: Write the header**

Create `source/core/feed/feed_json.hpp`:

```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// --- Request body builders (pure) ---
std::string build_credentials_body(const std::string& user, const std::string& pass);
std::string build_refresh_body(const std::string& refreshToken);
std::string build_like_body(bool liked);
std::string build_comment_body(const std::string& text);

// --- Response parsers (added in Task 3) ---
struct RefreshResult { bool ok = false; std::string token; std::string refreshToken; };

AuthResult           parse_auth_response(const std::string& body, long status);
RefreshResult        parse_refresh_response(const std::string& body, long status);
FeedPage             parse_feed_page(const std::string& body);
std::vector<Comment> parse_comments(const std::string& body);
std::optional<Post>  parse_post(const std::string& body);

} // namespace thomaz::feed
```

- [ ] **Step 4: Write the builder implementations**

Create `source/core/feed/feed_json.cpp`:

```cpp
#include "core/feed/feed_json.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>

namespace thomaz::feed {

using nlohmann::json;

std::string build_credentials_body(const std::string& user, const std::string& pass)
{
    return json{ {"username", user}, {"password", pass} }.dump();
}

std::string build_refresh_body(const std::string& refreshToken)
{
    return json{ {"refreshToken", refreshToken} }.dump();
}

std::string build_like_body(bool liked)
{
    return json{ {"liked", liked} }.dump();
}

std::string build_comment_body(const std::string& text)
{
    return json{ {"text", text} }.dump();
}

// Parsers added in Task 3.

} // namespace thomaz::feed
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cd tests && make test`
Expected: PASS — the four builder cases pass.

- [ ] **Step 6: Commit**

```bash
git add source/core/feed/feed_json.hpp source/core/feed/feed_json.cpp tests/test_feed_json.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(auth): add feed_json request-body builders

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: feed_json response parsers

Defensive, non-throwing parsers for every API response the client consumes. `gameTitleId` arrives as a hex **string**; `createdAt` is epoch-seconds number; missing fields never throw.

**Files:**
- Modify: `source/core/feed/feed_json.cpp`
- Test: `tests/test_feed_json.cpp` (extend)

- [ ] **Step 1: Write the failing tests**

Append to `tests/test_feed_json.cpp`:

```cpp
TEST_CASE("parse_auth_response success fills token + refreshToken") {
    auto r = parse_auth_response(R"({"ok":true,"token":"acc","refreshToken":"ref"})", 200);
    CHECK(r.ok);
    CHECK(r.token == "acc");
    CHECK(r.refreshToken == "ref");
}

TEST_CASE("parse_auth_response 401 -> invalid credentials, no crash") {
    auto r = parse_auth_response(R"({"ok":false,"error":"invalid_credentials"})", 401);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("parse_auth_response 409 -> username exists") {
    auto r = parse_auth_response(R"({"ok":false,"error":"username_already_exists"})", 409);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("parse_auth_response transport failure (status 0)") {
    auto r = parse_auth_response("", 0);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
}

TEST_CASE("parse_auth_response garbage body does not throw") {
    auto r = parse_auth_response("not json", 200);
    CHECK_FALSE(r.ok);
}

TEST_CASE("parse_refresh_response success / failure") {
    auto ok = parse_refresh_response(R"({"ok":true,"token":"t2","refreshToken":"r2"})", 200);
    CHECK(ok.ok); CHECK(ok.token == "t2"); CHECK(ok.refreshToken == "r2");
    auto bad = parse_refresh_response(R"({"ok":false,"error":"invalid_refresh_token"})", 401);
    CHECK_FALSE(bad.ok);
    CHECK_FALSE(parse_refresh_response("garbage", 200).ok);
}

TEST_CASE("parse_feed_page reads posts, gameTitleId hex string, createdAt") {
    const char* body = R"({
        "posts": [{
            "id":"p1",
            "author":{"id":"u1","username":"bea"},
            "imageUrl":"http://x/i.jpg",
            "caption":"hi",
            "gameTitleId":"0100000000010000",
            "gameName":"Mario",
            "likeCount":3,
            "likedByMe":true,
            "commentCount":2,
            "createdAt":1780000000
        }],
        "nextCursor":"abc",
        "hasMore":true
    })";
    auto page = parse_feed_page(body);
    REQUIRE(page.posts.size() == 1);
    const auto& p = page.posts[0];
    CHECK(p.id == "p1");
    CHECK(p.author.username == "bea");
    CHECK(p.gameTitleId == 0x0100000000010000ULL);
    CHECK(p.gameName == "Mario");
    CHECK(p.likeCount == 3);
    CHECK(p.likedByMe == true);
    CHECK(p.commentCount == 2);
    CHECK(p.createdAt == 1780000000);
    CHECK(page.nextCursor == "abc");
    CHECK(page.hasMore == true);
}

TEST_CASE("parse_feed_page tolerates missing fields and bad json") {
    auto empty = parse_feed_page("garbage");
    CHECK(empty.posts.empty());
    CHECK(empty.hasMore == false);

    auto partial = parse_feed_page(R"({"posts":[{"id":"p1"}],"hasMore":false})");
    REQUIRE(partial.posts.size() == 1);
    CHECK(partial.posts[0].id == "p1");
    CHECK(partial.posts[0].gameTitleId == 0); // missing -> 0
}

TEST_CASE("parse_comments reads list, tolerates bad json") {
    auto cs = parse_comments(R"([
        {"id":"c1","author":{"id":"u1","username":"kai"},"text":"gg","createdAt":1780000001}
    ])");
    REQUIRE(cs.size() == 1);
    CHECK(cs[0].id == "c1");
    CHECK(cs[0].author.username == "kai");
    CHECK(cs[0].text == "gg");
    CHECK(cs[0].createdAt == 1780000001);
    CHECK(parse_comments("nope").empty());
}

TEST_CASE("parse_post reads a single post, nullopt on bad json") {
    auto p = parse_post(R"({"post":{"id":"p9","caption":"yo","gameTitleId":"010000000E5EE000"}})");
    REQUIRE(p.has_value());
    CHECK(p->id == "p9");
    CHECK(p->caption == "yo");
    CHECK(p->gameTitleId == 0x010000000E5EE000ULL);
    CHECK_FALSE(parse_post("garbage").has_value());
}
```

> The comments endpoint shape: confirm whether `GET /posts/:id/comments` returns a bare JSON array or `{ "comments": [...] }`. Check `api/src/routes/posts.ts` (the comments GET handler) and make `parse_comments` match the real shape. The test above assumes a bare array — adjust both test and parser to the actual contract before implementing.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd tests && make test`
Expected: FAIL — parser functions are declared but not defined (link error) or return defaults.

- [ ] **Step 3: Implement the parsers**

Replace the `// Parsers added in Task 3.` comment in `source/core/feed/feed_json.cpp` with:

```cpp
namespace {

// Non-throwing parse: returns a discarded json on any error.
json safe_parse(const std::string& body)
{
    return json::parse(body, nullptr, /*allow_exceptions=*/false);
}

std::uint64_t parse_title_id_hex(const json& j, const char* key)
{
    if (!j.contains(key) || !j.at(key).is_string()) return 0;
    const std::string s = j.at(key).get<std::string>();
    if (s.empty()) return 0;
    return std::strtoull(s.c_str(), nullptr, 16);
}

User parse_user(const json& j)
{
    User u;
    if (j.is_object()) {
        u.id       = j.value("id", std::string{});
        u.username = j.value("username", std::string{});
    }
    return u;
}

Post parse_post_obj(const json& j)
{
    Post p;
    p.id           = j.value("id", std::string{});
    if (j.contains("author")) p.author = parse_user(j.at("author"));
    p.imageUrl     = j.value("imageUrl", std::string{});
    p.caption      = j.value("caption", std::string{});
    p.gameTitleId  = parse_title_id_hex(j, "gameTitleId");
    p.gameName     = j.value("gameName", std::string{});
    p.likeCount    = j.value("likeCount", 0);
    p.likedByMe    = j.value("likedByMe", false);
    p.commentCount = j.value("commentCount", 0);
    p.createdAt    = j.value("createdAt", static_cast<std::int64_t>(0));
    return p;
}

Comment parse_comment_obj(const json& j)
{
    Comment c;
    c.id        = j.value("id", std::string{});
    if (j.contains("author")) c.author = parse_user(j.at("author"));
    c.text      = j.value("text", std::string{});
    c.createdAt = j.value("createdAt", static_cast<std::int64_t>(0));
    return c;
}

// Maps a failed/non-2xx auth response to a stable, caller-displayable message.
// The activities show AuthResult.error verbatim when non-empty.
std::string auth_error_message(const json& j, long status)
{
    if (status == 0)   return "Sem conexão com o servidor.";
    if (status == 401) return "Usuário ou senha inválidos.";
    if (status == 409) return "Esse nome de usuário já existe.";
    if (j.is_object() && j.contains("error") && j.at("error").is_string())
        return j.at("error").get<std::string>();
    return "Falha ao autenticar. Tente novamente.";
}

} // namespace

AuthResult parse_auth_response(const std::string& body, long status)
{
    AuthResult r;
    json j = safe_parse(body);
    const bool twoxx = (status >= 200 && status < 300);
    if (twoxx && !j.is_discarded() && j.value("ok", false)) {
        r.ok           = true;
        r.token        = j.value("token", std::string{});
        r.refreshToken = j.value("refreshToken", std::string{});
        if (r.token.empty()) { r.ok = false; r.error = auth_error_message(j, status); }
        return r;
    }
    r.ok = false;
    r.error = auth_error_message(j.is_discarded() ? json::object() : j, status);
    return r;
}

RefreshResult parse_refresh_response(const std::string& body, long status)
{
    RefreshResult r;
    json j = safe_parse(body);
    const bool twoxx = (status >= 200 && status < 300);
    if (twoxx && !j.is_discarded() && j.value("ok", false)) {
        r.token        = j.value("token", std::string{});
        r.refreshToken = j.value("refreshToken", std::string{});
        r.ok           = !r.token.empty() && !r.refreshToken.empty();
    }
    return r;
}

FeedPage parse_feed_page(const std::string& body)
{
    FeedPage page;
    json j = safe_parse(body);
    if (j.is_discarded() || !j.is_object()) return page;
    if (j.contains("posts") && j.at("posts").is_array())
        for (const auto& jp : j.at("posts"))
            if (jp.is_object()) page.posts.push_back(parse_post_obj(jp));
    page.nextCursor = j.value("nextCursor", std::string{});
    page.hasMore    = j.value("hasMore", false);
    return page;
}

std::vector<Comment> parse_comments(const std::string& body)
{
    std::vector<Comment> out;
    json j = safe_parse(body);
    if (j.is_discarded()) return out;
    // Bare array shape. If the API wraps it as {"comments":[...]}, unwrap here
    // (see the contract note in the plan's Task 3 test step).
    const json* arr = nullptr;
    if (j.is_array()) arr = &j;
    else if (j.is_object() && j.contains("comments") && j.at("comments").is_array())
        arr = &j.at("comments");
    if (arr)
        for (const auto& jc : *arr)
            if (jc.is_object()) out.push_back(parse_comment_obj(jc));
    return out;
}

std::optional<Post> parse_post(const std::string& body)
{
    json j = safe_parse(body);
    if (j.is_discarded()) return std::nullopt;
    if (j.is_object() && j.contains("post") && j.at("post").is_object())
        return parse_post_obj(j.at("post"));
    if (j.is_object() && j.contains("id"))
        return parse_post_obj(j);
    return std::nullopt;
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd tests && make test`
Expected: PASS — all parser cases pass (after reconciling `parse_comments` with the real contract).

- [ ] **Step 5: Commit**

```bash
git add source/core/feed/feed_json.cpp tests/test_feed_json.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(auth): add defensive feed_json response parsers

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Extend IHttpClient to a general request API

Adds `HttpMethod`, `HttpRequest` (headers/body/multipart), and `request()` to the interface; keeps `get()` as a convenience shim; treats any 2xx as `ok()`.

**Files:**
- Modify: `source/platform/http_client.hpp`
- Test: none (interface-only; verified by Task 5's `FakeHttpClient` and the build). The `get()` shim keeps existing callers (`db_paths`, `self_update`) compiling.

- [ ] **Step 1: Rewrite the header**

Replace `source/platform/http_client.hpp` with:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace thomaz {

struct HttpResponse {
    long status = 0;       // HTTP status code (0 = transport/connection failure)
    std::string body;      // response body (the JSON document)

    bool ok() const { return status >= 200 && status < 300; }
};

enum class HttpMethod { Get, Post, Put, Delete };

struct MultipartFile {
    std::string field;       // form field name (e.g. "image")
    std::string filename;    // e.g. "post.jpg"
    std::string contentType; // e.g. "image/jpeg"
    std::vector<std::uint8_t> bytes;
};

struct HttpRequest {
    HttpMethod  method = HttpMethod::Get;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body; // JSON body; ignored when multipart fields/files are set

    // Multipart/form-data (used by createPost). When either is non-empty the
    // request is sent as multipart and `body` is ignored.
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<MultipartFile> files;
};

// HTTP abstraction so the UI/orchestration don't depend on libcurl.
class IHttpClient {
  public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse request(const HttpRequest& req) = 0;

    // Convenience GET kept for existing callers (db_paths/self_update).
    HttpResponse get(const std::string& url) {
        HttpRequest r;
        r.method = HttpMethod::Get;
        r.url    = url;
        return request(r);
    }
};

} // namespace thomaz
```

- [ ] **Step 2: Verify it compiles against existing callers**

Run: `grep -rn "->get(\|\.get(" source/core/db_paths.cpp source/platform/self_update.cpp source/core/update.cpp`
Expected: existing call sites use `get(url)` only — still satisfied by the shim. (No code change needed there; `CurlHttpClient::request` lands in Task 5.)

> Note: the build will not link until `CurlHttpClient` implements the new pure-virtual `request()` (Task 5). That's expected — commit the interface now; the build is exercised at the end of Task 5.

- [ ] **Step 3: Commit**

```bash
git add source/platform/http_client.hpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(http): extend IHttpClient with general request() + multipart

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: HttpFeedClient (with scripted FakeHttpClient tests)

The real `IFeedClient`. Stateful session, Bearer injection, refresh-once-on-401, persistence callbacks. Implement `CurlHttpClient::request()` in the same task so the desktop build links. Unit-tested against a `tests/`-only `FakeHttpClient` (no curl, no network).

**Files:**
- Create: `source/platform/feed/http_feed_client.hpp`
- Create: `source/platform/feed/http_feed_client.cpp`
- Modify: `source/platform/http_client_curl.cpp` (implement `request()`)
- Modify: `tests/Makefile` (add `../source/platform/feed/http_feed_client.cpp` to `SRCS`)
- Test: `tests/test_http_feed_client.cpp` (create, includes a `FakeHttpClient`)

- [ ] **Step 1: Add http_feed_client.cpp to the test Makefile SRCS**

In `tests/Makefile`, change the `SRCS` line to append the new client (it has no curl dependency, so it links in the test harness):

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/feed/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp
```

- [ ] **Step 2: Write the failing test (FakeHttpClient + flow)**

Create `tests/test_http_feed_client.cpp`:

```cpp
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
// supports a queue per key (FIFO) so a URL can answer 401 then 200, and
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
    // The login call itself carries no Authorization header.
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
    // gameTitleId is sent as a hex string field.
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
    // First addComment -> 401, then (after refresh) 200.
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
    // Exactly: comment(401), refresh(200), comment-retry(200) = 3 requests.
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
    // comment(401) + refresh(401) = 2 requests, then stop.
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

    // A 401 on a read must not kick off a refresh (only authedRequest does).
    FakeHttpClient http2;
    http2.push(HttpMethod::Get, BASE + "/feed", 401, R"({"ok":false})");
    bool lost = false;
    HttpFeedClient client2(&http2, BASE, restored, [](const feed::Session&){}, [&](){ lost = true; });
    client2.fetchFeed("");
    CHECK(http2.received.size() == 1); // no refresh attempt
    CHECK_FALSE(lost);
}
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — `http_feed_client.hpp` not found / `HttpFeedClient` undefined.

- [ ] **Step 4: Write the HttpFeedClient header**

Create `source/platform/feed/http_feed_client.hpp`:

```cpp
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
    // Injects Bearer from the session, refreshes once on 401, retries once.
    HttpResponse authedRequest(HttpRequest req);
    bool tryRefresh();                 // POST /auth/refresh; rotates + persists
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
```

- [ ] **Step 5: Write the HttpFeedClient implementation**

Create `source/platform/feed/http_feed_client.cpp`:

```cpp
#include "platform/feed/http_feed_client.hpp"
#include "core/feed/feed_json.hpp"
#include <cstdio>

namespace thomaz {

namespace {
std::string titleIdHex(std::uint64_t id) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016lx", static_cast<unsigned long>(id));
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

    // Access token likely expired: refresh once, then retry once.
    if (tryRefresh()) {
        // Replace the stale Authorization header with the fresh token.
        for (auto& h : req.headers)
            if (h.first == "Authorization") {
                std::lock_guard<std::mutex> lock(sessionMutex);
                h.second = "Bearer " + session.token;
            }
        return http->request(req);
    }

    // Refresh failed -> session is dead.
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
    return feed::parse_feed_page(resp.body);
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
    if (resp.ok) return ActionResult{ true, "" };
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
    if (resp.ok) return ActionResult{ true, "" };
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
    if (resp.ok) return ActionResult{ true, "" };
    AuthResult e = feed::parse_auth_response(resp.body, resp.status);
    return ActionResult{ false, e.error };
}

} // namespace thomaz
```

- [ ] **Step 6: Run the unit tests to verify they pass**

Run: `cd tests && make test`
Expected: PASS — all `test_http_feed_client.cpp` cases pass (login, multipart, refresh-on-401, refresh-fail, anonymous read, no-refresh-on-read-401), and all prior tests still pass.

- [ ] **Step 7: Implement CurlHttpClient::request()**

In `source/platform/http_client_curl.hpp`, change the override from `get` to `request` (keep the inherited `get` shim):

```cpp
// in class CurlHttpClient : public IHttpClient
HttpResponse request(const HttpRequest& req) override;
```

In `source/platform/http_client_curl.cpp`, replace the `HttpResponse CurlHttpClient::get(...)` definition with a `request()` that handles method, headers, body and multipart. Keep the existing `writeToString`, timeouts, user agent, and the `SSL_VERIFYPEER/HOST = 0` behavior:

```cpp
HttpResponse CurlHttpClient::request(const HttpRequest& req) {
    HttpResponse response;
    if (!networkReady)
        return response; // status 0 -> caller treats as network error

    CURL* curl = curl_easy_init();
    if (!curl)
        return response;

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Method.
    switch (req.method) {
        case HttpMethod::Get:    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); break;
        case HttpMethod::Post:   curl_easy_setopt(curl, CURLOPT_POST, 1L); break;
        case HttpMethod::Put:    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); break;
        case HttpMethod::Delete: curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); break;
    }

    // Headers.
    struct curl_slist* headerList = nullptr;
    for (const auto& h : req.headers) {
        std::string line = h.first + ": " + h.second;
        headerList = curl_slist_append(headerList, line.c_str());
    }
    if (headerList)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    // Body: multipart takes precedence over a raw body.
    curl_mime* mime = nullptr;
    const bool isMultipart = !req.files.empty() || !req.fields.empty();
    if (isMultipart) {
        mime = curl_mime_init(curl);
        for (const auto& f : req.fields) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, f.first.c_str());
            curl_mime_data(part, f.second.c_str(), CURL_ZERO_TERMINATED);
        }
        for (const auto& file : req.files) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, file.field.c_str());
            curl_mime_data(part, reinterpret_cast<const char*>(file.bytes.data()),
                           file.bytes.size());
            curl_mime_filename(part, file.filename.c_str());
            curl_mime_type(part, file.contentType.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    } else if (!req.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    else
        response.status = 0; // transport failure

    if (mime) curl_mime_free(mime);
    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return response;
}
```

> Remove the old `get()` definition from the `.cpp` (the base-class shim now provides it). Make sure `#include "platform/http_client_curl.hpp"` still declares only `request()` as the override.

- [ ] **Step 8: Verify the desktop build links and runs**

Run:
```bash
cmake -B build_desktop -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON >/dev/null
make -C build_desktop -j$(nproc)
timeout 5 ./build_desktop/thomaz; echo "exit=$?"
```
Expected: build succeeds; `exit=124` (healthy window loop). If `exit=139`, debug before continuing (run without timeout/redirect to see Borealis logs).

> The app still constructs `FakeFeedClient` at this point (wiring is Task 8), so this only verifies the new curl `request()` and interface link cleanly.

- [ ] **Step 9: Commit**

```bash
git add source/platform/feed/http_feed_client.hpp source/platform/feed/http_feed_client.cpp source/platform/http_client_curl.hpp source/platform/http_client_curl.cpp tests/test_http_feed_client.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(auth): add HttpFeedClient with refresh-on-401 + curl request()

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: API base URL setting (load/save + compiled default)

Adds `load_api_base_url()` / `save_api_base_url()` to `app_settings`, with a per-platform compiled default and a trimmed, trailing-slash-free override.

**Files:**
- Modify: `source/platform/app_settings.hpp`
- Modify: `source/platform/app_settings.cpp`
- Test: `tests/test_api_base_url.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `tests/test_api_base_url.cpp`:

```cpp
#include "doctest.h"
#include <cstdio>
#include "platform/app_settings.hpp"

using namespace thomaz;

TEST_CASE("api base url falls back to a non-empty default when none saved") {
    // No file saved in the test working dir -> compiled default.
    std::remove("thomaz-cache/api_url.txt");
    std::string def = load_api_base_url();
    CHECK_FALSE(def.empty());
    CHECK(def.find("://") != std::string::npos); // looks like a URL
}

TEST_CASE("api base url save trims and strips trailing slash, then loads back") {
    save_api_base_url("  http://localhost:3000/  ");
    CHECK(load_api_base_url() == "http://localhost:3000");
    std::remove("thomaz-cache/api_url.txt"); // cleanup
}
```

> `app_settings.cpp` is not currently in the test Makefile `SRCS`. Add it for this test: append `../source/platform/app_settings.cpp` to the `SRCS` line in `tests/Makefile`. It depends only on `cheat_store` (already in `SRCS`). Add `thomaz-cache/` and `test-saves-tmp/`-style artifacts to the Makefile `clean` target / `.gitignore` if not already ignored.

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — `load_api_base_url`/`save_api_base_url` undefined.

- [ ] **Step 3: Declare the functions**

In `source/platform/app_settings.hpp`, add below the locale declarations:

```cpp
// Returns the saved API base URL override, or the compiled default if none.
// Never has a trailing slash. Takes effect on next launch.
std::string load_api_base_url();

// Persist an API base URL override. Trims whitespace and any trailing slash.
// An empty value clears the override (falls back to the compiled default).
void save_api_base_url(const std::string& url);
```

- [ ] **Step 4: Implement the functions**

In `source/platform/app_settings.cpp`, add a file path, a default, and the two functions. Insert the `api_url_file()` helper next to `locale_file()`:

```cpp
std::string api_url_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/api_url.txt";
#else
    return "thomaz-cache/api_url.txt";
#endif
}

std::string default_api_base_url() {
#ifdef __SWITCH__
    // Production host (placeholder until the API is deployed). Override in Settings.
    return "https://thomaz-api.fly.dev";
#else
    return "http://localhost:3000";
#endif
}

std::string strip_trailing_slash(std::string s) {
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}
```

Then add the public functions (after `save_locale`):

```cpp
std::string load_api_base_url() {
    if (auto saved = read_text_file(api_url_file())) {
        std::string v = trim(*saved);
        if (!v.empty())
            return strip_trailing_slash(v);
    }
    return default_api_base_url();
}

void save_api_base_url(const std::string& url) {
    std::string v = strip_trailing_slash(trim(url));
    write_text_file(api_url_file(), v);
}
```

> `trim` is in the anonymous namespace of `app_settings.cpp` already — reuse it. Put `api_url_file`, `default_api_base_url`, and `strip_trailing_slash` in that same anonymous namespace.

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd tests && make test`
Expected: PASS — both base-url cases pass (the desktop default is `http://localhost:3000`).

- [ ] **Step 6: Commit**

```bash
git add source/platform/app_settings.hpp source/platform/app_settings.cpp tests/test_api_base_url.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(settings): add API base URL setting with compiled default

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: Settings UI — API URL field

Adds an editable "API URL" row to the settings screen, persisted via `save_api_base_url`. Follows the existing `InputCell`/`SelectorCell` pattern in `settings_activity.cpp`.

**Files:**
- Modify: `source/app/settings_activity.cpp`
- Modify: `resources/i18n/en-US/thomaz.json`, `resources/i18n/pt-BR/thomaz.json` (add 2 keys; other locales fall back per-key)
- Test: none (UI; verified by desktop build + manual check)

- [ ] **Step 1: Add i18n strings**

In `resources/i18n/pt-BR/thomaz.json`, inside the `settings` object, add:

```json
"api_url": "URL da API",
"api_url_hint": "Endereço do servidor (reinicie para aplicar)"
```

In `resources/i18n/en-US/thomaz.json`, inside the `settings` object, add:

```json
"api_url": "API URL",
"api_url_hint": "Server address (restart to apply)"
```

> Match the existing JSON structure exactly (find the `"settings"` object in each file). Borealis falls back to the default locale per-key, so fr/ru/zh-Hans need no change.

- [ ] **Step 2: Add the input row to settings**

In `source/app/settings_activity.cpp`, add the include near the others:

```cpp
#include <borealis/views/cells/cell_input.hpp>
```

Then, in `onContentAvailable()` after the language `SelectorCell` is added to `listBox`, add:

```cpp
    // --- API URL --------------------------------------------------------------
    auto* apiCell = new brls::InputCell();
    apiCell->init(
        "thomaz/settings/api_url"_i18n,
        load_api_base_url(),
        [](std::string v) {
            save_api_base_url(v);
            brls::Application::notify("thomaz/settings/saved"_i18n);
        },
        "thomaz/settings/api_url_hint"_i18n,
        "", 128);
    apiCell->addGestureRecognizer(new brls::TapGestureRecognizer(apiCell));
    listBox->addView(apiCell);
```

> Verify `InputCell::init`'s exact signature against `lib/borealis/.../cells/cell_input.hpp` and the existing usage in `auth_activity.cpp` (it calls `userCell->init(label, value, onChange, placeholder, "", maxLen)`). Match the real parameter order/count — adjust the call above if the signature differs.

- [ ] **Step 3: Verify the desktop build and the field renders**

Run:
```bash
make -C build_desktop -j$(nproc)
timeout 5 ./build_desktop/thomaz; echo "exit=$?"
```
Expected: build succeeds, `exit=124`. (Manual visual confirmation of the field happens during on-desktop testing.)

- [ ] **Step 4: Commit**

```bash
git add source/app/settings_activity.cpp resources/i18n/en-US/thomaz.json resources/i18n/pt-BR/thomaz.json
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(settings): add editable API URL field

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: Wire HttpFeedClient into main.cpp

Swap `FakeFeedClient` for `HttpFeedClient` on both platforms, injecting the existing `CurlHttpClient`, the resolved base URL, the restored session, and the persistence callbacks.

**Files:**
- Modify: `source/main.cpp`
- Test: none (integration; verified by desktop build + manual API handshake)

- [ ] **Step 1: Update includes in main.cpp**

In `source/main.cpp`, replace the feed-client include. Find where `FakeFeedClient`/`fake_feed_client.hpp` is included (near the other feed includes) and replace it with:

```cpp
#include "platform/feed/http_feed_client.hpp"
#include "platform/feed/auth_store.hpp"
#include "platform/app_settings.hpp"
```

> `app_settings.hpp` may already be included (locale). Don't duplicate — check first.

- [ ] **Step 2: Construct HttpFeedClient instead of FakeFeedClient**

In `source/main.cpp`, find the line that constructs the feed client (currently `auto feedClient = std::make_unique<thomaz::FakeFeedClient>();`) and replace it with:

```cpp
    // Community feed: real HTTP client against the thomaz-api. Base URL has a
    // compiled default (localhost on desktop, prod host on Switch) plus a
    // Settings override. The client owns the session and persists it via
    // auth_store on login/refresh; on refresh failure it clears the session.
    std::string apiBaseUrl = thomaz::load_api_base_url();
    auto restoredSession   = thomaz::load_session();
    auto feedClient = std::make_unique<thomaz::HttpFeedClient>(
        httpClient.get(),
        apiBaseUrl,
        restoredSession,
        [](const thomaz::feed::Session& s) { thomaz::save_session(s); },
        []() { thomaz::clear_session(); });
```

> This must come **after** `httpClient` is constructed (the `CurlHttpClient`). If the current order constructs `feedClient` before `httpClient`, move the feed-client construction below it. The `feedClient.get()` is passed to `HomeActivity`/`FeedActivity` exactly as before — no activity changes.

- [ ] **Step 3: Verify the desktop build and run**

Run:
```bash
make -C build_desktop -j$(nproc)
timeout 5 ./build_desktop/thomaz; echo "exit=$?"
```
Expected: build succeeds, `exit=124`. The app now targets `http://localhost:3000` on desktop; with no API running, feed/login calls fail gracefully (transport error → "Sem conexão" message), which is correct behavior.

- [ ] **Step 4: Manual end-to-end handshake (documented, not automated)**

Run the API and exercise the real flow:
```bash
cd api && docker compose up -d && npx prisma migrate deploy && npm run dev &
# in another shell:
make -C build_desktop -j$(nproc) && ./build_desktop/thomaz
```
Verify manually: register a new user, confirm the feed loads, create a post (image upload), like and comment. Confirm `api/uploads/` receives the JPEG and the post appears. Confirm the session persists across an app restart (no re-login needed).

> This step is a manual checklist, not an automated test. Record the result in the final summary.

- [ ] **Step 5: Commit**

```bash
git add source/main.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(auth): wire HttpFeedClient into main on both platforms

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Final verification (after all tasks)

- [ ] `cd tests && make test` — all suites green (session codec, feed_json builders+parsers, http_feed_client flow, api base url).
- [ ] Desktop build clean, `exit=124`.
- [ ] Manual API handshake checklist (Task 8 Step 4) recorded.
- [ ] Switch `.nro` builds in Docker (`devkitpro/devkita64`, `-DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON`, target `thomaz.nro`) — build only; on-console smoke test is the user's call.
- [ ] **No push, no tag** — local commits only until the user confirms stability.

## Notes for the implementer

- **`FakeFeedClient` stays in the tree** (still compiled by the CMake glob) but is no longer wired into `main`. Do not delete it — it's useful for offline runs and its construction shape documents the `AuthResult` brace-init order.
- **The `token` parameter** on `createPost`/`setLike`/`addComment` is intentionally ignored by `HttpFeedClient` (it uses its internal session). This is the one deliberate seam — keep the explanatory comments.
- **Contract checks before implementing parsers (Task 3):** confirm the comments-list shape and the createPost success shape against `api/src/routes/posts.ts`. The parsers tolerate both bare-array and wrapped shapes, but the tests assert one — make them match reality.
- **nlohmann include path:** match whatever existing `source/` code uses (`grep -rn "nlohmann/json" source/`).
