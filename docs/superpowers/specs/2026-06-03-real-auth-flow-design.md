# Real Auth Flow + HttpFeedClient — Design

**Date:** 2026-06-03
**Status:** Approved (design)
**Depends on:** the `thomaz-api` backend (`api/`, spec `2026-06-03-thomaz-api.md`) and the existing feed UI (`AuthActivity`, `FeedActivity`, `ComposerActivity`) which today run against `FakeFeedClient`.

## Goal

Replace the in-memory `FakeFeedClient` with a real network client (`HttpFeedClient`) that talks to the `thomaz-api` over HTTP, so login/register, the community feed, posting, likes and comments all run against the real backend. Sessions persist across launches with transparent access-token refresh.

## Scope

**In scope — the complete `IFeedClient` against the real API:**
- `registerUser`, `login` → `POST /auth/register`, `POST /auth/login`
- transparent token refresh via `POST /auth/refresh` (access token expires in 15 min; refresh token rotates)
- `fetchFeed` → `GET /feed?cursor=` (optionalAuth: sends Bearer when a session exists, works anonymously otherwise)
- `createPost` → `POST /posts` (multipart/form-data with the JPEG)
- `setLike` → `PUT /posts/:id/like`
- `fetchComments` → `GET /posts/:id/comments`, `addComment` → `POST /posts/:id/comments`
- session persistence (token + refreshToken + username) on disk
- API base URL: compiled default + optional override in Settings

**Out of scope (deferred):**
- Cloud save sync (`/saves/*`) — separate feature, already has its own `ISaveService`/Save Manager.
- A dedicated logout button beyond clearing the session on refresh failure (a Settings logout entry is optional, not required by this design).
- Shipping a CA bundle for strict TLS verification (the existing curl client skips peer/host verification; this design keeps that behavior — tracked as the same pre-existing TODO).
- End-to-end network mocking in CI. The real handshake is verified manually.

## Architecture

Follows the project's established pattern: a platform interface (`IHttpClient`), platform implementations (curl), pure testable logic in `core/`, and a feed client that composes them. The new real client mirrors how `ITitleService`/`ISaveService` have fake + real implementations, so it can be unit-tested with a scripted fake `IHttpClient` — no network required.

```
AuthActivity / FeedActivity / ComposerActivity   (unchanged UI)
            |  IFeedClient
            v
      HttpFeedClient  ── holds Session (in memory) + persist/auth-lost callbacks
            |                 |
   IHttpClient (request)   core/feed/feed_json   (pure parse/build, unit-tested)
            |
     CurlHttpClient  ── POST/PUT/DELETE/GET + headers + body + multipart
            |
        thomaz-api
```

## Components

### 1. `IHttpClient` — general request API
`source/platform/http_client.hpp` (modify)

Today: GET-only, no headers. Extend to a general request without breaking `get()`:

```cpp
enum class HttpMethod { Get, Post, Put, Delete };

struct MultipartFile {
    std::string field;          // form field name (e.g. "image")
    std::string filename;       // e.g. "post.jpg"
    std::string contentType;    // e.g. "image/jpeg"
    std::vector<std::uint8_t> bytes;
};

struct HttpRequest {
    HttpMethod  method = HttpMethod::Get;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers; // e.g. Authorization, Content-Type
    std::string body;                                         // JSON body (when not multipart)
    // Multipart form (used by createPost). When non-empty, `fields` + `files`
    // are sent as multipart/form-data and `body` is ignored.
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<MultipartFile> files;
};

class IHttpClient {
  public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse request(const HttpRequest& req) = 0;
    // Convenience GET kept for existing callers (db_paths/self_update).
    HttpResponse get(const std::string& url) {
        return request(HttpRequest{HttpMethod::Get, url, {}, {}, {}, {}});
    }
};
```

`HttpResponse` is unchanged (`status`, `body`, `ok()`), except `ok()` should accept any 2xx, not just 200 — the API returns 200 for all success cases today, but POST endpoints may return 200/201; treat `200 <= status < 300` as ok. (Verify against the API; current routes return 200.)

### 2. `CurlHttpClient` — implement `request()`
`source/platform/http_client_curl.{hpp,cpp}` (modify)

- Build a `curl_slist` from `req.headers`. For JSON bodies set `Content-Type: application/json` (unless caller already provided one).
- Method via `CURLOPT_CUSTOMREQUEST` (or `CURLOPT_POST`/`CURLOPT_UPLOAD` as appropriate); body via `CURLOPT_POSTFIELDS` + `CURLOPT_POSTFIELDSIZE`.
- Multipart: when `req.files`/`req.fields` are present, build a `curl_mime` (`curl_mime_init`, `curl_mime_addpart`, `curl_mime_name`, `curl_mime_data`/`curl_mime_filedata` via `curl_mime_data` with the in-memory bytes, `curl_mime_filename`, `curl_mime_type`) and attach with `CURLOPT_MIMEPOST`.
- Keep existing timeouts, user agent, and the `SSL_VERIFYPEER/HOST = 0` behavior (same TODO as today).
- `get()` delegates to `request()`.

### 3. `core/feed/feed_json` — pure parse/build
`source/core/feed/feed_json.{hpp,cpp}` (new). No IO, no curl. Uses `nlohmann::json` defensively.

```cpp
namespace thomaz::feed {

// Parsing (all non-throwing; bad input -> safe defaults)
AuthResult     parse_auth_response(const std::string& body, long status);
FeedPage       parse_feed_page(const std::string& body);
std::vector<Comment> parse_comments(const std::string& body);
std::optional<Post>  parse_post(const std::string& body);     // for createPost response
struct RefreshResult { bool ok; std::string token; std::string refreshToken; };
RefreshResult  parse_refresh_response(const std::string& body, long status);

// Building request bodies
std::string build_credentials_body(const std::string& user, const std::string& pass); // {username,password}
std::string build_refresh_body(const std::string& refreshToken);                       // {refreshToken}
std::string build_like_body(bool liked);                                               // {liked}
std::string build_comment_body(const std::string& text);                               // {text}

} // namespace thomaz::feed
```

Parsing rules:
- `json::parse(body, nullptr, /*allow_exceptions=*/false)`; `is_discarded()` → safe default (failed `AuthResult`, empty `FeedPage`, empty vector, `nullopt`).
- `gameTitleId` arrives as a **string** (the API serializes title IDs as hex strings). Parse with `strtoull(s, nullptr, 16)`; empty/invalid → `0`.
- `createdAt` is a number (epoch seconds) → read as `int64`, guarded.
- Every field read via `.value("field", default)` so missing fields never throw.
- Error mapping (in `parse_auth_response`): `status == 0` → transport/connection error message key; `401` → invalid credentials; `409` → username already exists; otherwise use the API's `error` string if present, else a generic i18n fallback. The activity already shows `r.error` verbatim when non-empty, so put a human/i18n-resolvable string there.

> Note: `AuthResult` currently lives in `feed_client.hpp` and carries a single `token`. Move/extend it so `feed_json` can populate `token` + `refreshToken` (add a `refreshToken` field to `AuthResult`, or have `parse_auth_response` return token+refresh and let the client assemble the `Session`). Keep `AuthResult`'s existing `ok`/`error`/`token` fields to avoid touching `FakeFeedClient` and the activities.

### 4. `Session` + codec — add refresh token
`source/core/feed/feed_types.hpp`, `source/core/feed/session_codec.cpp` (modify)

```cpp
struct Session {
    std::string token;        // access token (short-lived)
    std::string refreshToken; // long-lived, rotates on each refresh
    std::string username;
};
```

On-disk format becomes 3 lines: `token\nrefreshToken\nusername\n`.
**Backward compatibility:** if a file has only 2 lines (old `token\nusername`), `parse_session` returns a session with an empty `refreshToken`. The client treats an empty refresh token as "cannot refresh" → on the first 401 it triggers `onAuthLost` and the user logs in again once. No crash, no migration script.

### 5. `HttpFeedClient` — the real client
`source/platform/feed/http_feed_client.{hpp,cpp}` (new). Implements `IFeedClient`. Depends only on `IHttpClient` (abstract), `feed_json`, and `nlohmann::json` — **no curl include**, so it compiles and runs in the test harness.

```cpp
class HttpFeedClient : public IFeedClient {
  public:
    HttpFeedClient(IHttpClient* http,
                   std::string baseUrl,
                   std::optional<feed::Session> restored,
                   std::function<void(const feed::Session&)> onSessionChanged,
                   std::function<void()> onAuthLost);
    // ... IFeedClient overrides ...
  private:
    HttpResponse authedRequest(HttpRequest req);   // injects Bearer, refreshes on 401, retries once
    bool tryRefresh();                              // POST /auth/refresh, rotates + persists session
    IHttpClient* http;
    std::string  baseUrl;
    feed::Session session;        // source of truth for the token
    bool          hasSession = false;
    std::mutex    sessionMutex;   // guards session across worker-thread calls
    std::function<void(const feed::Session&)> onSessionChanged;
    std::function<void()> onAuthLost;
};
```

**Session is stateful and owned by the client** (the access token changes mid-flight on refresh, so the caller cannot own it).

- Constructor takes `restored` (session read from disk at boot, or `nullopt`).
- `onSessionChanged` → `main` wires to `save_session` (the client does no file IO — pure/platform split, same as the rest of `core`).
- `onAuthLost` → `main` wires to `clear_session`.

**The `token` parameter in `createPost`/`setLike`/`addComment` becomes vestigial.** To avoid refactoring `IFeedClient`, `FakeFeedClient`, and the four activities, the signatures stay; `HttpFeedClient` **ignores the passed token and uses its internal session token** as the source of truth. This is documented in a code comment as the one ugly seam.

**Authed request envelope (retry once on 401):**
```
authedRequest(req):
    lock(sessionMutex)
    req.headers += Authorization: Bearer <session.token>
    resp = http->request(req)
    if resp.status == 401 and !session.refreshToken.empty():
        if tryRefresh():                      # rotates session, persists
            req.headers[Authorization] = Bearer <new session.token>
            resp = http->request(req)         # retry EXACTLY once
        else:
            onAuthLost()                      # clear session, propagate error
    return resp

tryRefresh():
    resp = http->request(POST baseUrl/auth/refresh, body=build_refresh_body(session.refreshToken))
    r = parse_refresh_response(resp.body, resp.status)
    if r.ok:
        session = { r.token, r.refreshToken, session.username }   # rotated pair
        hasSession = true
        onSessionChanged(session)
        return true
    return false
```

Behavior details:
- **Retry only once.** A persistent 401 after refresh propagates as an error — no loop.
- **Rotating refresh token:** persist the whole pair returned by `/auth/refresh`; the old refresh token is invalidated server-side.
- `login`/`registerUser`: build the credentials body, POST, `parse_auth_response`. On success set the session and call `onSessionChanged`. They do **not** go through the authed envelope (no token yet).
- `fetchFeed`/`fetchComments` (read, optionalAuth): send the Bearer **only if** a session exists (for `likedByMe`); anonymous otherwise. A 401 on an anonymous read does **not** trigger refresh.
- `createPost`: multipart — `files`=[{"image", "post.jpg", "image/jpeg", jpeg}], `fields`=[{"caption",...},{"gameTitleId", hex string},{"gameName",...}], through the authed envelope. Parse the returned post (or treat 2xx as success).
- `setLike`/`addComment`: JSON body through the authed envelope.
- Concurrency: each activity fires one call at a time via `brls::async`, but two screens could overlap; `sessionMutex` serializes session reads/mutations so a concurrent refresh can't corrupt the token.

### 6. API base URL configuration
`source/platform/app_settings.{hpp,cpp}` (modify), `source/app/settings_activity.*` + XML/i18n (modify)

```cpp
std::string load_api_base_url();           // saved override, else compiled default
void        save_api_base_url(const std::string&);
```

- Compiled default via `#ifdef`: desktop → `http://localhost:3000`, Switch → the production host (placeholder constant until the host is live).
- An empty saved override means "use the default".
- Settings gets an "API URL" input field (same `InputCell` pattern as the locale row), persisted like the locale. Takes effect on next launch (the client is constructed at startup with the resolved base URL).
- `HttpFeedClient` always builds full URLs as `baseUrl + "/auth/login"` etc.; `baseUrl` carries no trailing slash (trim on save).

### 7. Wiring
`source/main.cpp` (modify)

- Construct a `CurlHttpClient` (already exists for cheats — reuse the same instance).
- Resolve `baseUrl = load_api_base_url()`.
- `restored = load_session()`.
- Construct `HttpFeedClient(httpClient.get(), baseUrl, restored, save_session, clear_session)` on **both** platforms (it's plain HTTP via curl; works on desktop against localhost too).
- Pass it where `FakeFeedClient` is passed today.
- `FakeFeedClient` stays in the tree for tests/offline but is no longer wired into `main`.

## Data flow (login → post)

1. User opens Feed without a session → `FeedActivity` pushes `AuthActivity`.
2. User submits → `HttpFeedClient::login` → `POST /auth/login` → `{ok,token,refreshToken}` → session set, `save_session` called → `onAuthed` pops back.
3. `FeedActivity::fetchFeed` → `GET /feed` with Bearer → posts with `likedByMe`.
4. 15 min later the user comments → `addComment` → 401 → `tryRefresh` → `POST /auth/refresh` → new pair saved → comment retried with new token → success. User never notices.
5. If the refresh token is also expired → `tryRefresh` fails → `onAuthLost` clears the session → next feed entry asks for login again.

## Error handling

- Transport failure (`status == 0`): activities already display `r.error`; map to a "no connection" i18n string.
- Malformed/empty JSON: defensive parse → safe default, never crash.
- `401` (login) → invalid credentials; `409` (register) → username exists; other non-2xx → API `error` string or generic fallback.
- Refresh failure → session cleared, error surfaced to the calling screen, login requested.

## Testing

doctest, via `tests/Makefile`. The Makefile already globs `../source/core/feed/*.cpp`, so `feed_json.cpp` is picked up automatically; **add `../source/platform/feed/http_feed_client.cpp` to `SRCS`** (it has no curl dependency).

**`tests/test_feed_json.cpp`** — pure functions:
- `parse_auth_response`: `{ok,token,refreshToken}` → populated; `{ok:false,error}` → error; garbage body → `ok=false`, no crash; status mapping (0/401/409).
- `parse_feed_page`: full posts incl. `gameTitleId` string→u64 and `createdAt`; `nextCursor`/`hasMore`; empty page; malformed → empty page.
- `parse_comments`, `parse_post`: full + missing-field cases.
- `parse_refresh_response`: ok / failure / garbage.
- builders: `build_credentials_body`, `build_refresh_body`, `build_like_body`, `build_comment_body` — round-trip parse to assert shape.

**`tests/test_http_feed_client.cpp`** — client flow with a scripted `FakeHttpClient` (test-only, implements `IHttpClient`; returns canned responses keyed by (method,url) and records received requests):
- `login` ok → session populated, `onSessionChanged` called with token+refreshToken, no Authorization header on the login call itself.
- `createPost` → sends `Bearer <token>` and a multipart request with the expected fields/file.
- **Refresh on 401:** first authed call returns 401 → client calls `POST /auth/refresh` → retried call sends the **new** token and succeeds; `onSessionChanged` receives the rotated pair; assert the retry happened **exactly once**.
- **Refresh failure:** 401 + `/auth/refresh` returns 401 → `onAuthLost` called, error propagated, **no loop**.
- `fetchFeed` without session → no Authorization header, succeeds; with session → sends Bearer.
- 401 on an anonymous read → does **not** trigger refresh.
- `session_codec`: 3-line round-trip; 2-line legacy file → session with empty `refreshToken`.

The `FakeHttpClient` lives in `tests/` only. `FakeFeedClient` and its existing tests stay intact.

**Not automated** (documented, run manually): the real handshake against a running API (`cd api && npm run dev`, desktop pointed at `http://localhost:3000` via the Settings override) and an on-console smoke test. No end-to-end network mock in CI.

## File summary

| File | Action |
|---|---|
| `source/platform/http_client.hpp` | modify — `HttpMethod`, `HttpRequest`, multipart, `request()`, 2xx `ok()` |
| `source/platform/http_client_curl.{hpp,cpp}` | modify — implement `request()` (headers, body, `curl_mime`) |
| `source/core/feed/feed_json.{hpp,cpp}` | new — pure parse/build |
| `source/core/feed/feed_types.hpp` | modify — `Session.refreshToken` |
| `source/core/feed/session_codec.cpp` | modify — 3-line format + legacy fallback |
| `source/platform/feed/feed_client.hpp` | modify — `AuthResult.refreshToken` |
| `source/platform/feed/http_feed_client.{hpp,cpp}` | new — real `IFeedClient` |
| `source/platform/app_settings.{hpp,cpp}` | modify — API base URL load/save + default |
| `source/app/settings_activity.*`, `resources/xml/...`, `resources/i18n/...` | modify — API URL field + strings |
| `source/main.cpp` | modify — construct & wire `HttpFeedClient` |
| `tests/test_feed_json.cpp` | new |
| `tests/test_http_feed_client.cpp` | new (+ `FakeHttpClient`) |
| `tests/Makefile` | modify — add `http_feed_client.cpp` to `SRCS` |
