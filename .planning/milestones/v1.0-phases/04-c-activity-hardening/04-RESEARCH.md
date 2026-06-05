# Phase 4: C++ Activity Hardening - Research

**Researched:** 2026-06-05
**Domain:** Borealis (xfangfang fork) C++ activity lifetime/concurrency hardening; libcurl transfer cancellation; doctest host testing
**Confidence:** HIGH (all findings verified against vendored source in-repo; no external/training-data dependency)

## Summary

This phase is a pure internal robustness refactor of the `source/app/` activity layer. Every claim below is `[VERIFIED: codebase grep/read]` against the vendored Borealis (`lib/borealis/`) and project source — there are **no external packages to install**, so the Package Legitimacy, Environment Availability, and Validation-Architecture-framework-install concerns do not apply. (`nyquist_validation` is `false` in `.planning/config.json`, so the Validation Architecture section is omitted per the agent contract.)

The four requirements decompose cleanly but have **two non-obvious sharp edges** that the planner must design around:

1. **CONC-03 spans two distinct curl transport surfaces, not one.** The reference `xferInfo`/`CURLOPT_XFERINFOFUNCTION` lives in `source/platform/mods/mod_download.cpp` (the *file-download* path, used by **both mods and themes** via `download_file`). But cloud-save push/status and all *browse* traffic (mods, themes) flow through a **completely separate** code path — `source/platform/http_client_curl.cpp::CurlHttpClient::request()` — which has **no progress callback at all**. Wiring CONC-03 "into all activity-owned network transfers" (D-03) therefore means adding an xferInfo hook to **two** curl sites and threading the cancelled flag through **two** different signatures (`download_file(...)` and `IHttpClient::request(...)`).

2. **The `runAsync` dropped-callback test (D-05b) cannot instantiate a `ThomazActivity`.** The host doctest suite (`tests/Makefile`) compiles `source/core/**` plus a hand-picked list of `source/platform/*.cpp` — it does **not** compile `source/app/` or link Borealis. Since `ThomazActivity : brls::Activity`, a host test cannot construct one. The not-alive dropped-callback semantics must be extracted into a **pure, Borealis-free helper** (e.g. a free function in `source/core/` or an inline in a header that takes the guard + two `std::function`s) that both `runAsync` and the test call.

**Primary recommendation:** Build `ThomazActivity` (D-01a) owning `alive` + `cancelled` as `shared_ptr<atomic<bool>>`; have `runAsync` delegate the guard decision to a pure testable helper; thread `cancelled` into curl via a **per-transfer context struct** carrying a captured `shared_ptr<atomic<bool>>` (matching the existing `ProgressCtx` shape in `mod_download.cpp`); and for D-05a, test the `classify`→`plan_push` decision composition (the conflict *decision*, which is currently untested as a unit even though `plan_push` alone is covered).

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| `alive` lifetime guard (CONC-02) | App (`ThomazActivity` base) | — | Lifetime is bound to the activity object; base class owns it |
| `runAsync` wrapper (CONC-02) | App (`ThomazActivity` base) | Core (pure guard helper) | Wrapper lives on activity; the *testable* decision extracts to core |
| `cancelled` flag (CONC-03) | App (owns flag) | Platform (curl checks it) | Flag set by activity dtor; consumed in platform curl callback |
| In-flight transfer abort (CONC-03) | Platform (`mod_download.cpp` + `http_client_curl.cpp`) | — | Only curl's XFERINFOFUNCTION can abort an in-flight `curl_easy_perform` |
| View cast safety (DEBT-03) | App (4 named activities) | — | `getView()` is a Borealis Activity method; casts happen in activities |
| Conflict-decision test (TEST-04a) | Core (`save_sync`) | Tests | `classify`/`plan_push` are already pure core functions |
| Dropped-callback test (TEST-04b) | Core (extracted helper) | Tests | Must be Borealis-free to be host-testable |

## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Migrate **ALL** direct `brls::async` activity call-sites to `ThomazActivity::runAsync`, not just the three named in CONC-02. `brls::async` appears in **13 activity files** (verified — see "13-file inventory" below). Mirrors Phase 3 D-04 "all call-sites" precedent.
- **D-01a (S2):** `runAsync` lives on a new `source/app/thomaz_activity.hpp` `ThomazActivity` base class (inherits `brls::Activity`). It owns the `alive` guard, **removing the per-activity `alive` member** (e.g. `save_detail_activity.hpp:60`). Phase 3's CONC-01 atomicized `cloudBusy` in that header, so editing it is safe now.
- **D-02:** Unify both lifetime mechanisms in the base class. `ThomazActivity` owns BOTH `alive` (CONC-02) and `cancelled` (CONC-03). `runAsync` wires both; the curl `CURLOPT_XFERINFOFUNCTION` callback checks the base-class `cancelled` flag and returns 1 to abort.
- **D-03:** Wire the `cancelled`-flag check into **all activity-owned network transfers** (mods, cloud saves, themes), not just `mod_download.cpp`.
- **D-04:** On teardown abort, **drop silently** — no error UI or toast. Happy-path (non-cancelled) transfers MUST be unaffected (callback returns 0 normally).
- **D-05:** Cover **both** branches: (a) a host doctest for the cloud-save conflict-resolution / `plan_push` branch, AND (b) a host unit test for `runAsync`'s dropped-callback (not-alive) semantics.

### Claude's Discretion
- Exact `ThomazActivity` API shape, header layout, and `runAsync(worker, onSync)` signature/ownership (how `alive` + `cancelled` are captured and exposed to the curl layer).
- The mechanics of bridging the base-class `cancelled` flag into each platform-layer curl call site (shared_ptr handoff, per-transfer context struct, etc.).
- `dynamic_cast` null-handling specifics (log message form, early-return placement) — the safe-no-op contract is fixed; the form is open.
- Ordering/serialization of the DEBT-03 cast edits vs CONC-02 edits within the four shared activity files.

### Deferred Ideas (OUT OF SCOPE)
- None. (Two non-gating Phase-3 carryovers remain on the milestone hardware checklist, unrelated to this phase: forced-latch TLS-banner desktop smoke; Switch-toolchain build for IN-03 `uid_from_hex`.)

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| CONC-02 | `ThomazActivity::runAsync` base wrapper auto-captures `alive`; all 13 async call-sites migrate | 13-file inventory mapped; current capture idiom documented (`mod_browser_activity.cpp:53-69`); base-class layout + pure-helper extraction specified |
| CONC-03 | In-flight curl aborts on dtor via shared `cancelled` flag checked in XFERINFOFUNCTION | Both curl surfaces mapped (`mod_download.cpp` xferInfo present; `http_client_curl.cpp` has none); per-transfer context-struct handoff specified; happy-path return-0 preserved |
| DEBT-03 | C-style view casts → null-guarded `dynamic_cast` in 4 named activities | Exact cast lines enumerated for game_list / save_manager / save_detail / mod_browser; `View::cast<T>()` confirmed absent |
| TEST-04 | Host doctest for conflict/`plan_push` branch + `runAsync` dropped-callback semantics | `plan_push` conflict branch already partly covered; gap is the decision *composition*; dropped-callback needs a Borealis-free helper to be testable |

## Project Constraints (from CLAUDE.md / memory)

No `./CLAUDE.md` exists at repo root. No `.claude/skills/` or `.agents/skills/` present. Binding constraints come from CONVENTIONS.md, ARCHITECTURE.md, and persistent memory:

- **C++ standard:** App/NRO + desktop targets build with **C++20** (`-std=c++20`, per ARCHITECTURE.md:181). **But the host test suite (`tests/Makefile`) builds with `-std=c++17 -Wall -Wextra`.** Any code reached by a host test must compile under C++17. `[VERIFIED: tests/Makefile]`
- **Desktop build invariant:** `-DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` (else GLFW/wayland-scanner fails). Verification = clean build + host doctest suite. `[VERIFIED: memory + CMakeLists.txt:56,88]`
- **Naming:** C++ files `snake_case.{cpp,hpp}`; free functions `snake_case`; types/structs `PascalCase`; namespace `thomaz::` (core under `thomaz::core`). `#pragma once`, no include guards. `[CITED: CONVENTIONS.md]`
- **No exceptions** in core/platform code; return-value error semantics (`bool` + `std::string* outError`, or status enums). `[CITED: CONVENTIONS.md, ARCHITECTURE.md]`
- **Architecture direction:** app → platform → core (strict). Activities own no business logic; logic that needs host-test coverage must live in `source/core/`. `[CITED: ARCHITECTURE.md anti-patterns]`
- **Logging:** Use `brls::Logger::info/warn/error()` in NRO/app code (the natural fit for DEBT-03 null-guard log lines). `[CITED: ARCHITECTURE.md]`
- **Git identity:** commit as `luizfbalves <luizzbanndera@gmail.com>` (NOT the ambient userEmail). `[VERIFIED: memory]`
- **Worktrees disabled:** `use_worktrees: false` persisted in config (origin/main behind local main). `[VERIFIED: config.json + memory]`

## Standard Stack

No new dependencies. Everything used is already vendored or linked.

### Core (already present)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Borealis (xfangfang fork) | vendored `lib/borealis/` | UI framework; `brls::Activity`, `brls::async`/`brls::sync`, `getView()` | Project's only UI layer |
| libcurl | system/devkitPro | HTTP + file download; XFERINFOFUNCTION cancellation | Existing transport |
| doctest | vendored `lib/doctest/` | Host unit tests | Existing test framework (`tests/`) |
| nlohmann/json | vendored `lib/json/` | JSON | Existing |
| `<atomic>` / `<memory>` | C++ stdlib | `shared_ptr<atomic<bool>>` guards | Existing idiom (every activity header) |

**Installation:** None. `[VERIFIED: no package changes — pure refactor]`

## Architecture Patterns

### System Architecture Diagram (Phase-4 relevant data flow)

```text
            activity destruction (~ThomazActivity)
                          │ sets alive=false AND cancelled=true
                          ▼
   ┌─────────────────────────────────────────────────────────┐
   │  ThomazActivity (NEW base : brls::Activity)              │
   │  - shared_ptr<atomic<bool>> alive                       │
   │  - shared_ptr<atomic<bool>> cancelled                   │
   │  - runAsync(worker, onSync)                             │
   └───────────┬──────────────────────────────┬─────────────┘
               │ captures alive (by value)     │ captures cancelled (by value)
               ▼                                ▼
   ┌────────────────────────┐      ┌──────────────────────────────────┐
   │ brls::async(worker)    │      │ network transfer call            │
   │   → brls::sync:        │      │ (receives cancelled shared_ptr)  │
   │   guard helper decides │      └───────────────┬──────────────────┘
   │   run/skip onSync      │                      │
   └────────────────────────┘        ┌─────────────┴──────────────┐
        ▲                            ▼                            ▼
        │ pure, Borealis-free   download_file()           IHttpClient::request()
        │ guard helper          (mod_download.cpp)        (http_client_curl.cpp)
        │ (host-testable)       HAS xferInfo today        NO progress hook today
        │                            │                            │
        └── TEST-04b targets    add cancelled check      ADD xferInfo + cancelled
            this helper         (return 1 to abort)      check (return 1 to abort)
                                       ▲                         ▲
                          mods + themes file dl       cloud saves push/status,
                          (download_file shared)       mods/themes browse (GET)
```

### Recommended structure (new/changed files)
```
source/app/
├── thomaz_activity.hpp     # NEW: ThomazActivity base (alive + cancelled + runAsync)
├── *_activity.{hpp,cpp}    # 13 files migrate: inherit ThomazActivity, drop alive member,
│                           #   call runAsync, remove hand-rolled dtor alive=false
source/core/                # NEW pure helper for the dropped-callback guard (host-testable)
│   └── (e.g.) async_guard.hpp   # inline run_if_alive(guard, onSync) — Borealis-free
source/platform/
├── mods/mod_download.cpp        # add cancelled check to existing xferInfo
├── http_client_curl.cpp        # ADD xferInfo + cancelled check (currently none)
└── http_client.hpp             # extend request() to accept optional cancelled flag
tests/
├── test_save_sync.cpp          # extend: classify→plan_push decision composition (TEST-04a)
└── test_async_guard.cpp        # NEW: dropped-callback semantics (TEST-04b)
```

### Pattern 1: ThomazActivity base owning both guards
**What:** A single base class between each activity and `brls::Activity`, owning `alive` and `cancelled` as `shared_ptr<atomic<bool>>`, with a destructor that sets `alive=false; cancelled=true;`.
**When to use:** All 13 migrating activities.
**Example (current hand-rolled idiom to encapsulate):**
```cpp
// Source: source/app/mod_browser_activity.cpp:53-72  [VERIFIED: read]
auto alive       = this->alive;            // capture shared_ptr by value
brls::async([this, alive, http, tid, name]() {
    /* ... worker runs on pool thread, must not touch `this` directly ... */
    brls::sync([this, alive, g, mods]() {
        if (!alive->load()) return;        // ← the guard runAsync must make automatic
        this->onResolved(g, mods);
    });
});
```
Destructor idiom today: `*this->alive = false;` (`mod_browser_activity.cpp:33`, and 11 other dtors). `[VERIFIED: grep]`

### Pattern 2: runAsync signature + pure guard delegation
**What:** `runAsync(worker, onSync)` captures `alive` (and `cancelled`) by value before dispatch, runs `worker` on the async pool, then `brls::sync`-posts a wrapper that calls a **pure, Borealis-free** helper to decide whether `onSync` runs.
**Why split:** The guard *decision* (`if alive: run onSync else: drop`) is the only part TEST-04b needs to verify, and it must compile without Borealis. Extract it:
```cpp
// Source: proposed source/core/async_guard.hpp (Borealis-free, C++17-clean)
namespace thomaz::core {
// Returns true and runs onSync if the guard is still alive; returns false and
// drops onSync otherwise. Pure — no Borealis, host-testable.
inline bool run_if_alive(const std::shared_ptr<std::atomic<bool>>& alive,
                         const std::function<void()>& onSync) {
    if (!alive || !alive->load()) return false;
    onSync();
    return true;
}
}
```
`runAsync` (in `thomaz_activity.hpp`, app-layer, may use Borealis) then becomes:
```cpp
// Proposed shape — Claude's discretion on exact signature
template <class Worker, class OnSync>
void runAsync(Worker worker, OnSync onSync) {
    auto alive = this->alive;            // capture by value
    brls::async([alive, worker = std::move(worker),
                 onSync = std::move(onSync)]() {
        worker();                        // pool thread; no `this`
        brls::sync([alive, onSync]() {
            thomaz::core::run_if_alive(alive, onSync);  // ← tested decision
        });
    });
}
```
**Confidence:** HIGH that this split is necessary; the *exact* template/`std::function` shape is Claude's discretion per D-01a.

### Pattern 3: cancelled flag → curl via per-transfer context struct
**What:** Mirror the existing `ProgressCtx` struct (`mod_download.cpp:18-20`). Add the captured `shared_ptr<atomic<bool>> cancelled` to the context; the xferInfo callback returns `1` to abort when `cancelled->load()` is true, else `0` (unchanged happy path).
**Example (extend the existing callback):**
```cpp
// Source: source/platform/mods/mod_download.cpp:18-33  [VERIFIED: read]
struct ProgressCtx {
    const std::function<void(std::uint64_t, std::uint64_t)>* cb;
    std::shared_ptr<std::atomic<bool>> cancelled;   // NEW
};
int xferInfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<ProgressCtx*>(p);
    if (ctx && ctx->cancelled && ctx->cancelled->load())
        return 1;                       // NEW: abort in-flight transfer (D-03)
    /* ... existing progress reporting, returns 0 ... */
    return 0;
}
```
**For `http_client_curl.cpp` (no callback today):** add a `ProgressCtx`-equivalent + `CURLOPT_NOPROGRESS 0` + `CURLOPT_XFERINFOFUNCTION` + `CURLOPT_XFERINFODATA`, and extend `HttpRequest` (or `request()`'s signature) to carry the optional cancelled flag. The cleanest minimal change: add `std::shared_ptr<std::atomic<bool>> cancelled;` to `HttpRequest` (defaults null = never cancels, preserving all existing callers). `[VERIFIED: http_client.hpp / http_client_curl.cpp read]`

### Anti-Patterns to Avoid
- **Putting `runAsync`'s testable logic only in the app layer.** It then cannot be host-tested (Borealis not linked in `tests/`). Extract the guard decision to `source/core/`.
- **Threading `cancelled` as a raw `atomic<bool>*` or reference.** The activity may be destroyed while curl runs on a pool thread → dangling. Use `shared_ptr<atomic<bool>>` captured by value (same ownership model as `alive`). The flag object outlives the activity exactly because the worker holds a `shared_ptr` copy.
- **Returning non-zero from the happy-path xferInfo.** Any nonzero return aborts the transfer (`mod_download.cpp:32` comment confirms). Only return 1 when `cancelled` is set.
- **`brls::View::cast<T>()`** — does **not exist** in the vendored Borealis (confirmed: zero `cast` hits in `view.hpp`). Use `dynamic_cast<T*>` + null guard. `[VERIFIED: grep view.hpp]`

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Aborting an in-flight HTTP transfer | A separate watcher thread / signal | curl `CURLOPT_XFERINFOFUNCTION` returning 1 | The only in-pool-thread abort curl offers; `brls::async` has no cancellation handle (verified `thread.hpp`) |
| Activity-lifetime guard | A new bespoke mechanism | The existing `shared_ptr<atomic<bool>> alive` idiom, lifted into `ThomazActivity` | 13 activities already use it identically; just centralize |
| Task cancellation in the async pool | Trying to dequeue/kill a pool task | The `cancelled` flag (work still runs to completion, but the curl transfer aborts early and the `alive` guard drops the UI callback) | Borealis pool is fixed-size, no task handles (`thread.hpp:115` is a bare `vector<function>`) |

**Key insight:** Borealis' `async` is fire-and-forget (`thread.cpp:73`, pushes onto `m_async_tasks`); there is **no** future/handle/cancel-token. CONC-03's shared-flag design is therefore the *correct and only* viable approach — confirmed, not assumed.

## Runtime State Inventory

This is a code-only refactor (rename of inheritance + flag plumbing). No stored data, services, OS registrations, secrets, or build artifacts embed any phase-specific string.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — no datastore keys/IDs change. | None |
| Live service config | None — no external service config references activity class names. | None |
| OS-registered state | None — no Task Scheduler/pm2/systemd entries reference these symbols. | None |
| Secrets/env vars | None — no env var or secret key names change. | None |
| Build artifacts | New `thomaz_activity.hpp` + new core helper + new test file are added to the build. The **host test Makefile (`tests/Makefile`) hard-codes its source list** — adding a new `source/core/*.cpp` is auto-globbed (`$(wildcard ../source/core/*.cpp)`), but a new header-only inline helper needs no Makefile change; a new test `.cpp` in `tests/` is auto-globbed (`$(wildcard *.cpp)`). **No Makefile edit needed if the helper is header-only and lives in `source/core/`.** `[VERIFIED: tests/Makefile]` |

**Verified by:** grep of config.json, Makefile source list, and absence of any external registration referencing `*Activity` symbols.

## Common Pitfalls

### Pitfall 1: The two-curl-surface trap (CONC-03)
**What goes wrong:** Adding the cancelled check only to `mod_download.cpp` (the visible xferInfo) leaves **cloud-save uploads and all browse GETs uncancellable**, silently failing D-03's "all activity-owned network transfers."
**Why it happens:** `mod_download.cpp` is the only file that *already* has an xferInfo, so it looks like "the" transfer path. But cloud saves (`http_cloud_save_client.cpp` → `IHttpClient::request`) and mods/themes browse use `http_client_curl.cpp`, which has no progress hook at all.
**How to avoid:** Plan **two** curl edits: (1) extend `mod_download.cpp::xferInfo`, (2) **add** a new xferInfo to `http_client_curl.cpp::request` and extend `HttpRequest` to carry the flag.
**Warning signs:** A plan that touches only `mod_download.cpp` for CONC-03.

### Pitfall 2: Host test cannot link Borealis (TEST-04b)
**What goes wrong:** Writing a `runAsync` test that `#include`s `thomaz_activity.hpp` / `borealis.hpp` → link failure (Borealis not in `tests/Makefile` source list; the suite is C++17, app code is C++20).
**Why it happens:** `tests/Makefile` compiles only `source/core/**` + a curated platform-file list; no `source/app/`, no Borealis, `-std=c++17`.
**How to avoid:** Extract the dropped-callback decision into `source/core/async_guard.hpp` (header-only, C++17-clean, no Borealis). Test *that*. `runAsync` becomes a thin app-layer wrapper that delegates to it.
**Warning signs:** A test plan that `#include`s any `borealis` header or `source/app/*`.

### Pitfall 3: `plan_push` conflict branch is *already* tested (TEST-04a redundancy)
**What goes wrong:** Writing a duplicate `plan_push(CloudAhead) → isConflict==true` test adds no coverage — `tests/test_save_sync.cpp` "plan_push for each situation" already asserts exactly that.
**Why it happens:** D-05a says "conflict-resolution / `plan_push` branch," which sounds untested but isn't, at the unit level.
**How to avoid:** Target the genuine gap: the **decision composition** `classify(...) → plan_push(...)` as the `doUpload` path computes it (`save_detail_activity.cpp:275-283`). Add cases that drive a realistic `(cloudExists, cloudRev, syncedRev)` triple through both functions and assert the resulting `(revision, isConflict)` — i.e. the regression guard for the *combined* decision the UI relies on. The UI dialog wiring itself stays untestable (Borealis), which is acceptable.
**Warning signs:** A new test that only calls `plan_push` with a hand-set `SyncSituation`.

### Pitfall 4: DEBT-03 scope creep
**What goes wrong:** C-style `(brls::X*)this->getView(...)` casts are pervasive (~30+ sites across all activities). Replacing all of them balloons the diff and risks unrelated churn.
**Why it happens:** grep shows casts everywhere; the instinct is to "fix them all."
**How to avoid:** DEBT-03 is scoped to **exactly four files** per CONTEXT/CONCERNS: `game_list_activity.cpp`, `save_manager_activity.cpp`, `save_detail_activity.cpp`, `mod_browser_activity.cpp`. Do not touch casts in the other 9.
**Warning signs:** Cast edits in `theme_detail`, `auth_activity`, `mod_manager`, `cheat_detail`, `settings`, etc.

### Pitfall 5: Removing per-activity `alive` member breaks 12 destructors
**What goes wrong:** Each of the 12 activities with a destructor does `*this->alive = false;`. After moving `alive` to `ThomazActivity` (with its dtor setting it), leaving the per-activity dtor line referencing a removed member won't compile; deleting the dtor entirely may drop other cleanup.
**Why it happens:** `alive` is declared in each `.hpp` and set in each `.cpp` dtor independently (verified: 12 dtors set `alive=false`).
**How to avoid:** Per file: remove the `alive` member from the `.hpp`, change base to `ThomazActivity`, and remove the `*this->alive = false;` line from the dtor (base dtor now handles it) — but **keep** any other dtor body. Some activities may have *only* that line in their dtor → the dtor can then be removed entirely (or left empty/defaulted). Plan this as a mechanical per-file checklist.
**Warning signs:** Compile errors `'alive' is not a member`; or a base-class `alive=false` that runs *after* a derived dtor that still needed it (order is fine: derived dtor runs first, base sets flag last — acceptable since the flag's only consumers are async callbacks).

## Code Examples

### The 13-file `brls::async` inventory (D-01 migration targets) `[VERIFIED: grep]`
| File | `brls::async` line(s) | Has dtor `alive=false`? | DEBT-03 target? |
|------|----------------------|------------------------|-----------------|
| `mod_detail_activity.cpp` | 75, 206 | yes | no |
| `clear_cheats_activity.cpp` | 41 | yes | no |
| `auth_activity.cpp` | 98 | yes | no |
| `theme_browser_activity.cpp` | 94, 115 | yes | no |
| `game_list_activity.cpp` | 74, 242 | yes | **YES** (84-87) |
| `mod_browser_activity.cpp` | 58, 166, 191 | yes | **YES** (45, 208-209) |
| `save_manager_activity.cpp` | 36 | yes | **YES** (48-49) |
| `save_detail_activity.cpp` | 167, 192, 262, 309, 356, 476 | yes | **YES** (87, 90, 422) |
| `settings_activity.cpp` | 145, 195, 217 | yes | no |
| `theme_detail_activity.cpp` | 57, 74, 141, 179, 203 | yes | no |
| `cheat_detail_activity.cpp` | 47 | yes | no |
| `mod_manager_activity.cpp` | (no direct async; has dtor) | yes | no |
| `home_activity` / `system_activity` | none | (no alive) | no |

> Note: CONTEXT lists 13 files including both `save_detail_activity.hpp` AND `.cpp`. The `.hpp` change is the `alive` member removal + base change; the `.cpp` holds the 6 async sites. `mod_manager_activity.cpp` has a dtor + `alive` but its async usage is indirect — confirm during planning whether it needs a `runAsync` migration or only the base-class swap. `[VERIFIED: grep — mod_manager has alive member + dtor but no direct brls::async line in the grep]`

### DEBT-03 exact cast sites in the 4 target files `[VERIFIED: grep]`
```cpp
// game_list_activity.cpp:86-87
brls::Box* listBox      = (brls::Box*)this->getView("gameListBox");
brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");

// save_manager_activity.cpp:48-49
brls::Box* listBox      = (brls::Box*)this->getView("saveListBox");
brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");

// save_detail_activity.cpp:87, 90, 422
if (auto* lbl = (brls::Label*)this->getView("lastBackup")) ...
brls::Box* box = (brls::Box*)this->getView("historyBox");
if (auto* lbl = (brls::Label*)this->getView("cloudStatus")) ...

// mod_browser_activity.cpp:47, 84, 99, 135, 208-209  (multiple Label/Box casts)
if (auto* emptyLabel = (brls::Label*)this->getView("emptyLabel")) ...
auto* resultsBox = (brls::Box*)this->getView("resultsBox");
```
**Replacement contract (DEBT-03):** wrong-type/mistyped id fails safe (log + return), never a bad-pointer crash later:
```cpp
auto* listBox = dynamic_cast<brls::Box*>(this->getView("gameListBox"));
if (!listBox) {
    brls::Logger::error("gameListBox missing or not a Box");   // form: Claude's discretion
    return;                                                     // safe no-op
}
```
Note: sites already written as `if (auto* x = (brls::T*)getView(...))` only need the inner cast swapped to `dynamic_cast` (the null guard already exists). Bare assignments (e.g. `game_list:86-87`, `save_manager:48-49`, `save_detail:90`) need an *added* null guard.

### TEST-04a — decision-composition test (extend `tests/test_save_sync.cpp`) `[VERIFIED: existing test read]`
```cpp
// classify -> plan_push composition (the decision doUpload relies on)
TEST_CASE("upload decision: cloud advanced since last sync => conflict") {
    auto sit  = classify(/*cloudExists=*/true, /*cloudRev=*/5, /*syncedRev=*/3);
    auto plan = plan_push(sit, /*cloudRev=*/5);
    CHECK(sit == SyncSituation::CloudAhead);
    CHECK(plan.isConflict);
    CHECK(plan.revision == 5);
}
TEST_CASE("upload decision: in sync => clean push, no conflict") {
    auto sit  = classify(true, 3, 3);
    auto plan = plan_push(sit, 3);
    CHECK_FALSE(plan.isConflict);
    CHECK(plan.revision == 3);
}
```

### TEST-04b — dropped-callback test (new `tests/test_async_guard.cpp`)
```cpp
#include "doctest.h"
#include "core/async_guard.hpp"
#include <atomic>
#include <memory>
using namespace thomaz::core;

TEST_CASE("onSync runs when alive") {
    auto alive = std::make_shared<std::atomic<bool>>(true);
    bool ran = false;
    bool r = run_if_alive(alive, [&]{ ran = true; });
    CHECK(r); CHECK(ran);
}
TEST_CASE("onSync dropped when not alive (activity popped)") {
    auto alive = std::make_shared<std::atomic<bool>>(true);
    *alive = false;                       // simulate ~ThomazActivity
    bool ran = false;
    bool r = run_if_alive(alive, [&]{ ran = true; });
    CHECK_FALSE(r); CHECK_FALSE(ran);
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Per-activity hand-captured `alive` + manual `if(!alive->load())return;` | Centralized `ThomazActivity::runAsync` auto-guard | This phase | Guard impossible to forget |
| In-flight curl runs to completion after activity popped | `cancelled` flag aborts via XFERINFOFUNCTION | This phase | Frees pool threads on teardown |
| C-style `(brls::T*)getView()` | `dynamic_cast<brls::T*>` + null guard | This phase (4 files) | Fail-safe on layout/id mismatch |

**Deprecated/outdated:** `brls::View::cast<T>()` was hypothesized by the requirement text (DEBT-03 mentions it as an option) but **does not exist** in this vendored Borealis — only `dynamic_cast` is viable. `[VERIFIED: grep view.hpp]`

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `mod_manager_activity.cpp` needs only the base-class swap (no direct `brls::async` to migrate) — grep found its `alive`/dtor but no `brls::async` line. | 13-file inventory | LOW — if it has indirect async, planner adds it to runAsync migration; verified by re-grep at plan time. |
| A2 | Adding `shared_ptr<atomic<bool>> cancelled` to `HttpRequest` (defaulting null) is the cleanest handoff for the `http_client_curl.cpp` surface vs. a separate signature param. | Pattern 3 | LOW — both work; this minimizes caller churn. Claude's discretion per D-02. |
| A3 | Extracting the guard decision to a header-only `source/core/async_guard.hpp` needs no `tests/Makefile` edit (header-only + `tests/*.cpp` auto-globbed). | Runtime State Inventory | LOW — if a `.cpp` is used instead, `$(wildcard ../source/core/*.cpp)` still auto-globs it. |

## Open Questions

1. **Does `mod_manager_activity.cpp` have an async path needing migration?**
   - What we know: it declares `alive` + a dtor setting it (`grep`), but no `brls::async` line surfaced.
   - What's unclear: whether it dispatches async indirectly (e.g. via a helper).
   - Recommendation: planner re-greps `mod_manager_activity.cpp` for `async`/`sync` at plan time; if none, it only swaps base class + drops the `alive` member.

2. **Exact `runAsync` worker→onSync data handoff.**
   - What we know: the worker must not touch `this`; results pass via captured copies (current idiom captures result structs into the `brls::sync` lambda).
   - What's unclear: whether `runAsync` should pass the worker's return value into `onSync` (typed) or leave callers to capture, as today.
   - Recommendation: Claude's discretion (D-01a). A `runAsync<T>(worker→T, onSync(T))` typed form is cleaner but more template-heavy; the simpler `runAsync(worker(), onSync())` with caller-captured results matches the existing pattern with least churn.

## DEBT-03 / CONC-02 shared-file edit ordering (planner coordination)

Four files are touched by **both** DEBT-03 (casts) and CONC-02 (base swap + `alive` removal): `game_list`, `save_manager`, `save_detail`, `mod_browser`. To avoid merge churn, edit each file **once, completely** rather than in two passes:

**Recommended per-file order (single pass):**
1. `.hpp`: change base `public brls::Activity` → `public ThomazActivity`; remove the `alive` member.
2. `.cpp`: remove `*this->alive = false;` from the dtor (and drop the dtor if it becomes empty).
3. `.cpp`: migrate each `brls::async([...alive...]{...})` site to `runAsync(...)`.
4. `.cpp`: in the same file, swap that file's DEBT-03 C-style casts to `dynamic_cast` + null guard.

**Sequencing across the phase:** Implement CONC-02 (`ThomazActivity` + `async_guard.hpp`) **first** (roadmap + D-02), then CONC-03 builds on the base's `cancelled` flag, then DEBT-03 casts (mechanical, independent), then TEST-04. Treat the four shared files as atomic units so the cast and async edits land together.

## Sources

### Primary (HIGH confidence — all in-repo, verified by read/grep)
- `source/platform/mods/mod_download.cpp` — xferInfo/ProgressCtx reference shape (CONC-03)
- `source/platform/http_client_curl.cpp` + `http_client.hpp` — the second, hook-less curl surface
- `source/platform/saves/http_cloud_save_client.cpp` — cloud push/status route through `IHttpClient`
- `source/app/mod_browser_activity.cpp`, `game_list_activity.cpp`, `save_manager_activity.cpp`, `save_detail_activity.{hpp,cpp}` — alive idiom, dtors, DEBT-03 cast sites
- `lib/borealis/library/include/borealis/core/{view,thread,activity}.hpp` + `lib/core/thread.cpp` — confirmed no `View::cast`, no async cancellation handle
- `tests/Makefile`, `tests/test_save_sync.cpp`, `tests/test_save_sync_state.cpp` — test build scope (C++17, no Borealis), existing coverage
- `source/core/saves/save_sync.{hpp,cpp}` — `classify`/`plan_push` (TEST-04a)
- `.planning/config.json` — `nyquist_validation:false`, `use_worktrees:false`

### Secondary / context
- `.planning/codebase/{CONCERNS,CONVENTIONS,ARCHITECTURE}.md`; `.planning/REQUIREMENTS.md`; `.planning/phases/04-c-activity-hardening/04-CONTEXT.md`

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new deps; everything vendored/verified in-repo.
- Architecture (base class + two-surface curl + pure-helper extraction): HIGH — derived directly from read source, not training data.
- Pitfalls: HIGH — each pitfall is a concrete, grep/read-verified fact (two curl surfaces, no-Borealis test build, plan_push already tested, 4-file DEBT scope, 12 dtors).
- Test reachability: HIGH — `tests/Makefile` source list read directly.

**Research date:** 2026-06-05
**Valid until:** Stable — in-repo facts don't drift; re-verify only if Borealis is re-vendored or `tests/Makefile` scope changes.
