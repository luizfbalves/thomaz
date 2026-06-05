# Phase 4: C++ Activity Hardening - Pattern Map

**Mapped:** 2026-06-05
**Files analyzed:** 22 (1 new base header, 1 new core helper, 13 activity migrations, 2 curl surfaces + 1 http header, 2 test files)
**Analogs found:** 21 / 22 (1 net-new helper has a close shape analog)

> All excerpts below are verified by direct read of the cited files. Line numbers are from the current tree.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `source/app/thomaz_activity.hpp` (NEW) | provider/base | event-driven (async lifetime) | `source/app/save_detail_activity.hpp` (alive member + dtor idiom) | role-match (new base, lifts existing idiom) |
| `source/core/async_guard.hpp` (NEW) | utility (pure helper) | transform (guard decision) | `source/core/saves/save_sync.{hpp,cpp}` (pure core decision fns) | role-match (same pure-core, host-testable shape) |
| `source/app/mod_browser_activity.{hpp,cpp}` | component (activity) | event-driven + request-response | itself (canonical alive idiom 53-72) + DEBT-03 casts | exact |
| `source/app/game_list_activity.cpp` | component (activity) | event-driven + CRUD list | `mod_browser_activity.cpp` | exact |
| `source/app/save_manager_activity.cpp` | component (activity) | event-driven + CRUD list | `mod_browser_activity.cpp` | exact |
| `source/app/save_detail_activity.{hpp,cpp}` | component (activity) | event-driven + request-response | `mod_browser_activity.cpp` | exact |
| `save_detail/mod_detail/theme_*/auth/settings/cheat_detail/clear_cheats/mod_manager` (9 more) | component (activity) | event-driven | `mod_browser_activity.cpp` | exact (base-swap + runAsync only, no DEBT-03) |
| `source/platform/mods/mod_download.cpp` | service (transport) | streaming/file-I/O | itself (existing `xferInfo`/`ProgressCtx` 18-33) | exact (extend in place) |
| `source/platform/http_client_curl.cpp` | service (transport) | request-response | `mod_download.cpp` xferInfo block (62-66) | role-match (port the hook in) |
| `source/platform/http_client.hpp` | config/contract | — | itself (`HttpRequest` struct 25-35) | exact (add optional field) |
| `tests/test_save_sync.cpp` | test | transform | itself (existing `classify`/`plan_push` cases) | exact (extend) |
| `tests/test_async_guard.cpp` (NEW) | test | transform | `tests/test_save_sync.cpp` | exact (same doctest shape) |

## Pattern Assignments

### `source/app/thomaz_activity.hpp` (NEW base, event-driven)

**Analog:** `source/app/save_detail_activity.hpp` (the `alive` member it absorbs) + `source/app/mod_browser_activity.cpp:31-34` (the dtor flag-set it absorbs).

**Member idiom to lift (from `save_detail_activity.hpp:60`):**
```cpp
std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
```
The base adds a sibling `cancelled` of the same type. Header already includes `<atomic>` + `<memory>` (`save_detail_activity.hpp:4-5`) — replicate those includes.

**Destructor idiom to centralize (from `mod_browser_activity.cpp:31-34`):**
```cpp
ModBrowserActivity::~ModBrowserActivity()
{
    *this->alive = false; // tell an in-flight UI callback to bail
}
```
Base dtor becomes: `*alive = false; *cancelled = true;` — this single dtor replaces the 12 hand-rolled per-activity ones.

**`runAsync` wrapper** wires the capture-by-value idiom currently hand-written at `mod_browser_activity.cpp:53-72` (see below) and delegates the guard decision to `thomaz::core::run_if_alive`. Exact template/`std::function` signature is Claude's discretion (D-01a). May use Borealis (`brls::async`/`brls::sync`) since it is app-layer.

---

### `source/core/async_guard.hpp` (NEW pure utility, transform)

**Analog:** `source/core/saves/save_sync.hpp` — the established "pure, Borealis-free, host-testable core decision function" shape. Must compile under **C++17** (host test build) and use namespace `thomaz::core`, `#pragma once`.

**Pattern to produce (Borealis-free decision):**
```cpp
namespace thomaz::core {
inline bool run_if_alive(const std::shared_ptr<std::atomic<bool>>& alive,
                         const std::function<void()>& onSync) {
    if (!alive || !alive->load()) return false;
    onSync();
    return true;
}
}
```
Why split: `tests/Makefile` compiles `source/core/**` only — never `source/app/` or Borealis. The guard *decision* is the only TEST-04b-observable part and must live here. Header-only inline => no `tests/Makefile` edit needed (`source/core/*.cpp` is auto-globbed; a header needs nothing).

---

### `source/app/mod_browser_activity.{hpp,cpp}` (activity — CANONICAL, event-driven + request-response)

**Analog:** itself. This is the reference idiom for all 13 migrations AND a DEBT-03 target.

**Capture+guard idiom being encapsulated by `runAsync` (lines 53-72):**
```cpp
auto alive       = this->alive;            // capture shared_ptr by value
IHttpClient* http = this->http;
std::uint64_t tid = this->title.title_id;  // copy — worker must not touch `this`
std::string name  = this->title.name;

brls::async([this, alive, http, tid, name]() {
    core::GameResolve g = core::resolve_game(tid, name, fetch);
    /* ... worker on pool thread, no `this` ... */
    brls::sync([this, alive, g, mods]() {
        if (!alive->load()) return;        // ← the guard runAsync makes automatic
        this->onResolved(g, mods);
    });
});
```
Migration: replace the `auto alive = this->alive;` + `brls::async(...brls::sync(...if(!alive->load())return...))` scaffold with a `runAsync(worker, onSync)` call. Worker keeps the by-value captures (no `this`); `onSync` body is what ran after the guard.

**`.hpp` change:** base `public brls::Activity` → `public ThomazActivity`; **remove** the `alive` member.
**`.cpp` dtor:** remove `*this->alive = false;` (line 33); drop the dtor if it becomes empty (base handles it).

**DEBT-03 casts in this file** (`mod_browser_activity.cpp:47, 84, 99, 135, 208-209`). Example existing site (line 47):
```cpp
if (auto* emptyLabel = (brls::Label*)this->getView("emptyLabel"))  // C-style cast
```
Replace inner cast with `dynamic_cast<brls::Label*>(...)`. Sites already wrapped in `if (auto* x = ...)` only need the cast swapped (null guard exists). Bare-assignment sites need an **added** null guard (see contract below).

---

### `source/app/game_list_activity.cpp` / `save_manager_activity.cpp` / `save_detail_activity.{hpp,cpp}` (activities — base-swap + runAsync + DEBT-03)

**Analog:** `mod_browser_activity.cpp` (all four share the identical idiom).

**Single-pass per-file edit order** (avoids DEBT-03/CONC-02 merge churn — these 4 files are touched by both):
1. `.hpp`: base → `ThomazActivity`; remove `alive` member.
2. `.cpp` dtor: remove `*this->alive = false;` (drop dtor if empty).
3. `.cpp`: migrate each `brls::async` site to `runAsync`.
4. `.cpp`: swap that file's DEBT-03 C-style casts to `dynamic_cast` + null guard.

**Exact DEBT-03 cast sites** (verified, scoped to these 4 files ONLY):
```cpp
// game_list_activity.cpp:86-87   (bare assignments — ADD null guard)
brls::Box*   listBox    = (brls::Box*)this->getView("gameListBox");
brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");

// save_manager_activity.cpp:48-49  (bare assignments — ADD null guard)
brls::Box*   listBox    = (brls::Box*)this->getView("saveListBox");
brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");

// save_detail_activity.cpp:87 (guarded), :90 (bare), :422 (guarded)
brls::Box* box = (brls::Box*)this->getView("historyBox");  // :90 bare — ADD guard
```
`async` sites: game_list 74,242 · save_manager 36 · save_detail 167,192,262,309,356,476.

---

### Remaining 9 activities (base-swap + runAsync only — NO DEBT-03)

**Analog:** `mod_browser_activity.cpp`. Files + async lines: `mod_detail`(75,206) · `clear_cheats`(41) · `auth`(98) · `theme_browser`(94,115) · `settings`(145,195,217) · `theme_detail`(57,74,141,179,203) · `cheat_detail`(47) · `mod_manager`(re-grep: alive+dtor but no direct async — likely base-swap only). Do **not** touch casts here (Pitfall 4).

---

### `source/platform/mods/mod_download.cpp` (transport — streaming, EXTEND existing hook)

**Analog:** itself (`ProgressCtx` 18-20, `xferInfo` 22-33, setopt 64-66). Verified shape:
```cpp
struct ProgressCtx {
    const std::function<void(std::uint64_t, std::uint64_t)>* cb;
};
int xferInfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<ProgressCtx*>(p);
    if (ctx && ctx->cb && *ctx->cb) { /* ... */ }
    return 0; // nonzero would abort
}
// setup (64-66):
curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfo);
curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
```
**Edit:** add `std::shared_ptr<std::atomic<bool>> cancelled;` to `ProgressCtx`; at the top of `xferInfo` add `if (ctx && ctx->cancelled && ctx->cancelled->load()) return 1;` (abort). Happy path still returns 0. Thread the flag into `download_file(...)`'s signature (defaulted/optional).

---

### `source/platform/http_client_curl.cpp` + `http_client.hpp` (transport — request-response, ADD hook)

**Analog:** the `mod_download.cpp` xferInfo block above — port it into `CurlHttpClient::request()`, which currently has **no** progress callback. This is the second curl surface (cloud-save push/status + all mods/themes browse GETs).

**Contract extension (`http_client.hpp:25-35`, `HttpRequest`):** add an optional, default-null field so all existing callers are unaffected (verified: `HttpRequest` is the single request struct, and `get()` at :44-49 builds one with no extra fields):
```cpp
struct HttpRequest {
    HttpMethod  method = HttpMethod::Get;
    std::string url;
    // ... existing fields ...
    std::shared_ptr<std::atomic<bool>> cancelled; // NEW — null = never cancels
};
```
Then in `request()`: build a `ProgressCtx`-equivalent carrying `req.cancelled`, set `CURLOPT_NOPROGRESS 0` + `CURLOPT_XFERINFOFUNCTION` + `CURLOPT_XFERINFODATA`, return 1 when `cancelled->load()`. Requires `<atomic>`/`<memory>` includes in `http_client.hpp`.

---

### `tests/test_save_sync.cpp` (test — extend, TEST-04a)

**Analog:** itself. Existing cases (`classify` 6-21, `plan_push` 23-35) confirm `plan_push` alone is already covered. Add the genuine gap: the `classify(...) → plan_push(...)` **composition** that `doUpload` relies on:
```cpp
TEST_CASE("upload decision: cloud advanced since last sync => conflict") {
    auto sit  = classify(/*cloudExists=*/true, /*cloudRev=*/5, /*syncedRev=*/3);
    auto plan = plan_push(sit, 5);
    CHECK(sit == SyncSituation::CloudAhead);
    CHECK(plan.isConflict);
    CHECK(plan.revision == 5);
}
```
Match existing style: `#include "doctest.h"` + `#include "core/saves/save_sync.hpp"`, `using namespace thomaz::core;`, plain `CHECK`.

---

### `tests/test_async_guard.cpp` (NEW test — TEST-04b)

**Analog:** `tests/test_save_sync.cpp` header/structure. Auto-globbed by `tests/Makefile` (`$(wildcard *.cpp)`) — no Makefile edit. Must be C++17-clean, include only `doctest.h` + `core/async_guard.hpp` (NO Borealis, NO `source/app/*`):
```cpp
#include "doctest.h"
#include "core/async_guard.hpp"
using namespace thomaz::core;

TEST_CASE("onSync runs when alive") {
    auto alive = std::make_shared<std::atomic<bool>>(true);
    bool ran = false;
    CHECK(run_if_alive(alive, [&]{ ran = true; }));
    CHECK(ran);
}
TEST_CASE("onSync dropped when not alive (activity popped)") {
    auto alive = std::make_shared<std::atomic<bool>>(true);
    *alive = false;
    bool ran = false;
    CHECK_FALSE(run_if_alive(alive, [&]{ ran = true; }));
    CHECK_FALSE(ran);
}
```

## Shared Patterns

### Lifetime guard ownership (CONC-02 / CONC-03)
**Source:** `save_detail_activity.hpp:60` (member) + `mod_browser_activity.cpp:33` (dtor flag-set).
**Apply to:** all 13 activities (member removed, lifted into `ThomazActivity`).
```cpp
std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
// dtor: *alive = false;  (+ *cancelled = true; in the new base)
```
Both `alive` and `cancelled` MUST be `shared_ptr<atomic<bool>>` captured by value into workers — the activity may die while a pool thread runs; the flag object outlives it via the worker's `shared_ptr` copy. **Never** pass raw `atomic<bool>*` or reference (Anti-Pattern: dangling).

### Curl cancellation via XFERINFOFUNCTION
**Source:** `mod_download.cpp:18-33, 64-66`.
**Apply to:** both curl surfaces (`mod_download.cpp`, `http_client_curl.cpp`).
- Return `1` ONLY when `cancelled->load()`; return `0` on the happy path (nonzero aborts — see `:32` comment).
- Carry the flag in a per-transfer `ProgressCtx`-style struct, never a global.

### DEBT-03 null-guarded cast contract
**Source:** none exists yet (this phase establishes it); `View::cast<T>()` confirmed absent — `dynamic_cast` is the only option.
**Apply to:** exactly the 4 named files (game_list, save_manager, save_detail, mod_browser) — NOT the other 9 (Pitfall 4).
```cpp
auto* listBox = dynamic_cast<brls::Box*>(this->getView("gameListBox"));
if (!listBox) {
    brls::Logger::error("gameListBox missing or not a Box"); // form: discretion
    return;                                                   // safe no-op
}
```
Log via `brls::Logger::error/warn` (app-layer convention).

### Pure-core / host-test reachability
**Source:** `source/core/saves/save_sync.hpp` + `tests/test_save_sync.cpp`.
**Apply to:** `async_guard.hpp` + `test_async_guard.cpp`. Anything a host test reaches must be C++17-clean, `thomaz::core`, Borealis-free, header-only or `source/core/*.cpp` (auto-globbed). No `#include <borealis...>` or `source/app/*` in any test.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| (none) | — | — | Every file has a strong in-repo analog; `async_guard.hpp` is net-new but mirrors the `save_sync` pure-core shape closely enough to be a role-match. |

## Metadata

**Analog search scope:** `source/app/` (activities), `source/platform/` (mods + http curl), `source/core/saves/`, `tests/`.
**Files scanned (read this pass):** `mod_download.cpp`, `mod_browser_activity.cpp`, `http_client.hpp`, `save_detail_activity.hpp`, `test_save_sync.cpp` (plus the verified RESEARCH.md grep inventory for the remaining 11 activities).
**Pattern extraction date:** 2026-06-05
