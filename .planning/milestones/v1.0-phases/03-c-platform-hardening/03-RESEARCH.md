# Phase 3: C++ Platform Hardening - Research

**Researched:** 2026-06-05
**Domain:** C++17/20 Switch homebrew (Borealis/brls TUI), curl TLS, filesystem helpers, doctest host tests, dual CMake(app)+Makefile(tests) build
**Confidence:** HIGH — almost all findings are direct reads of this repo's source and vendored borealis; verified by file:line.

## Summary

This phase is entirely internal C++ refactoring against THIS codebase — there is no external-library decision to make. The research value is in (1) the exact file:line facts the planner needs, (2) the behavioral diffs between the duplicate helpers so consolidation is provably behavior-preserving, and (3) the two viable banner-injection mechanisms with their tradeoffs.

The dominant non-obvious facts: the **app build uses `file(GLOB_RECURSE source/*.cpp)`** so `fs_util.cpp` is auto-picked-up by CMake (no edit), but the **`tests/Makefile` uses an explicit SRCS list** (no glob) so `fs_util.cpp` MUST be added there by hand. There is **no single shared root UI frame** — every activity instantiates its own `brls::AppletFrame` from XML — so a "global banner" is achieved either via a per-activity shared helper (the existing `install_header_username` precedent, `source/app/app_header.cpp`) or via a true overlay drawn above all activities (the `NotificationManager` precedent). For TEST-03, the `ca_ok` decision currently lives entirely inside `#ifdef __SWITCH__` in a header-only `inline` function, so it is not host-compilable as written — D-06's `tls_policy(bool)` extraction is mandatory to create a host-testable seam.

**Primary recommendation:** Create `source/platform/fs_util.{hpp,cpp}` with `ensure_parent_dirs` (substring-at-slash canonical form) and `copy_tree` (mod_store's `std::string* err` signature is the richer/canonical one). Extract `tls_policy(bool ca_present) -> {long verifypeer, long verifyhost}` as a free function OUTSIDE the `#ifdef`. Implement the SEC-03 banner as a shared `install_tls_warning_banner(activity)` helper following the exact `install_header_username` pattern, gated on a process-global `tls_is_insecure()` flag set when `ca_ok==false`. Atomicize `cloudBusy` with plain `load()`/`store()` (all accesses are main-thread; `compare_exchange` is unnecessary). Add `fs_util.cpp` to `tests/Makefile` SRCS.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| `ensure_parent_dirs` / `copy_tree` filesystem helpers | Platform (`source/platform/`) | — | I/O + POSIX `mkdir`/`opendir`; belongs in platform per CONVENTIONS (core = pure, platform = I/O) |
| `tls_policy(bool)` decision logic | Platform, but host-compilable (outside `#ifdef __SWITCH__`) | Tests link it directly | Pure function (no curl/Switch deps) so doctest can exercise it on host |
| `apply_curl_tls()` (sets curl opts) | Platform, Switch-specific branch | — | Touches `CURL*`; consumes `tls_policy` output |
| TLS insecure flag (process-global state) | Platform (`curl_tls`) sets it | App (UI) reads it | Flag is set where TLS degrades; UI reads it to render banner |
| TLS warning banner (UI) | App (`source/app/`) | — | Borealis view injection; UI concern only |
| `cloudBusy` concurrency guard | App (`save_detail_activity`) | — | UI-thread activity state |

## Standard Stack

No new external packages. This phase uses only what is already vendored/linked:

### Core (already present — verified by direct read)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| doctest | header-only (`lib/doctest/doctest.h`) | host unit tests (TEST-03) | already the project's C++ test runner; 37 `test_*.cpp` files |
| Borealis (xfangfang fork) | vendored `lib/borealis/` | TUI; `brls::Application`, `brls::AppletFrame`, `brls::Label`, `brls::Box` | the app's UI framework |
| libcurl | system (desktop) / switch-curl (Switch) | `apply_curl_tls` target | existing networking |
| `<atomic>` (libstdc++) | C++17 stdlib | `std::atomic<bool>` for `cloudBusy` | already `#include <atomic>` in `save_detail_activity.hpp` (the `alive` guard uses it) |

**Installation:** none. `std::atomic` header is already included at `source/app/save_detail_activity.hpp:4`.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `tls_policy` free function | inline ternary inside `apply_curl_tls` | rejected — D-06 requires a host-testable seam; inline ternary inside `#ifdef __SWITCH__` is not host-compilable |
| Shared per-activity banner helper | true overlay above activity stack | both viable; see "Pattern 1" tradeoffs below. Per-activity helper matches the established `install_header_username` precedent and is lower-risk |
| `load`/`store` for `cloudBusy` | `compare_exchange_strong` | `compare_exchange` only needed if a check-and-set must be atomic against another thread; here all sites are main-thread (see CONC-01 analysis), so plain load/store preserves exact current semantics |

## Package Legitimacy Audit

> Not applicable — this phase installs **zero** external packages. All code is internal refactoring against already-vendored/linked dependencies. slopcheck/registry verification skipped (nothing to verify).

## Architecture Patterns

### System Architecture Diagram (data/control flow for this phase)

```
                    ┌─────────────────────────────────────────────┐
  app startup       │ main.cpp: brls::Application::init()/createWindow │
  (Switch only) ───►│  → first apply_curl_tls(handle) probes CA   │
                    └───────────────┬─────────────────────────────┘
                                    │ ca_ok computed once (static)
                                    ▼
       ┌──────────────────────────────────────────────────────┐
       │ curl_tls.hpp                                          │
       │  tls_policy(ca_present) ──► {verifypeer, verifyhost}  │  ◄── TEST-03 host test
       │  apply_curl_tls() sets curl opts from policy          │      calls tls_policy(false)
       │  if !ca_present: set_tls_insecure_flag(true)          │
       └───────────────┬──────────────────────────────────────┘
                       │ process-global bool tls_is_insecure()
                       ▼
       ┌──────────────────────────────────────────────────────┐
       │ each Activity::onContentAvailable()                   │
       │   install_tls_warning_banner(this)  ◄── new helper    │
       │     if tls_is_insecure(): inject warning Label/Box    │
       │     into AppletFrame header (or push overlay)         │
       └──────────────────────────────────────────────────────┘

  fs_util consolidation (independent of the above):
       cheat_store / theme_install / libarchive_extractor / mod_download
       mod_store / mod_actions / save_service_switch
            │  all call ──► fs_util::ensure_parent_dirs / fs_util::copy_tree
            ▼
       source/platform/fs_util.{hpp,cpp}  (single definition)
            └─ linked by CMake app (auto via GLOB) AND tests/Makefile (manual add)
```

### Recommended File Layout
```
source/platform/
├── fs_util.hpp          # NEW: declares ensure_parent_dirs, copy_tree (thomaz:: namespace)
├── fs_util.cpp          # NEW: definitions; auto-picked by CMake GLOB, manual-add to tests/Makefile
└── curl_tls.hpp         # EDIT: extract tls_policy() outside #ifdef; set insecure flag

source/app/
├── tls_banner.{hpp,cpp} # NEW (recommended): install_tls_warning_banner() shared helper
│                        #   (mirror of existing app_header.{hpp,cpp})
└── save_detail_activity.{hpp,cpp}  # EDIT: cloudBusy -> std::atomic<bool>

tests/
├── test_tls_policy.cpp  # NEW: TEST-03 (ca_present==false branch)
├── test_fs_util.cpp     # NEW (recommended for D-05): proves the two ensure_parent_dirs equivalent
└── Makefile             # EDIT: add ../source/platform/fs_util.cpp to SRCS (and curl_tls is header-only, no add needed unless tls_policy lands in a .cpp)
```

### Pattern 1: Global UI element via per-activity shared helper (RECOMMENDED for banner)
**What:** A free function called from every activity's `onContentAvailable()` that reaches into the activity's `AppletFrame` header by bound view ID and injects a view. This is THE established pattern in this codebase.
**When to use:** the SEC-03 persistent banner.
**Precedent (verified):** `source/app/app_header.cpp` — `install_header_username(brls::Activity*)`:
```cpp
// Source: source/app/app_header.cpp (verified read)
void install_header_username(brls::Activity* activity) {
    if (!activity) return;
    // hint_box is the empty right-aligned slot in the AppletFrame header.
    auto* hintBox = dynamic_cast<brls::Box*>(
        activity->getView("brls/applet_frame/hint_box"));
    if (!hintBox) return;
    auto* lbl = new brls::Label();
    lbl->setText("@" + sess->username);
    lbl->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
    hintBox->addView(lbl);
}
```
Every one of the 14 activities already calls `install_header_username(this)` in `onContentAvailable` (verified: `home_activity.cpp:31`, plus 13 others). The TLS banner helper would be installed the same way. Available header slots (from `lib/borealis/library/include/borealis/views/applet_frame.hpp:84-89`, `BRLS_BIND`):
- `brls/applet_frame/header` (Box) — the whole top header bar
- `brls/applet_frame/hint_box` (Box) — right-aligned slot (already holds the username)
- `brls/applet_frame/title_label`, `brls/applet_frame/title_icon`, `brls/applet_frame/footer`

**Tradeoff vs overlay:** A per-activity helper means the banner is re-injected per screen (must be called in each activity's `onContentAvailable`). It is visible on every screen that calls it — which satisfies D-02 "global, visible across all screens" — but a NEW future activity that forgets the call would lack the banner (same fragility the `alive`-guard has). Mitigation: add the call to every existing activity AND document the requirement; or use Pattern 2.

### Pattern 2: True overlay above the activity stack (alternative for banner)
**What:** A single persistent view drawn ON TOP of all activities, like Borealis' own notifications.
**Precedent (verified):** `lib/borealis/library/lib/core/application.cpp:794` — the render loop draws the activities stack, then unconditionally `Application::notificationManager->frame(&frameContext)` on top (`NotificationManager` extends `Box`, positioned via `setTranslationX`/`setTranslationY`, see `notification_manager.cpp:27-32`). This is a genuinely app-global layer.
**Tradeoff:** There is no public Borealis API to register a *custom* persistent overlay (NotificationManager is private/`inline static` in `Application`). Achieving this would require either (a) hijacking `Application::notify()` (transient — rejected by D-02) or (b) a custom mechanism that re-draws each frame. Higher risk, no existing seam. **Recommend Pattern 1** unless the planner wants to invest in a custom always-on view.

### Pattern 3: Process-global insecure flag (REQUIRED regardless of banner mechanism)
**What:** `curl_tls.hpp` computes `ca_ok` once (`static const bool ca_ok = [...]`). When false, set a process-global flag the UI can read.
```cpp
// in curl_tls.hpp (host-safe — outside #ifdef)
namespace thomaz {
inline bool& tls_insecure_flag() { static bool f = false; return f; }
inline bool tls_is_insecure() { return tls_insecure_flag(); }
}
// inside #ifdef __SWITCH__ else-branch of apply_curl_tls:
tls_insecure_flag() = true;
```
The banner helper reads `tls_is_insecure()`. On desktop the flag stays false (desktop branch always verifies), so the banner never shows on host — correct.

### Anti-Patterns to Avoid
- **Putting `tls_policy` inside `#ifdef __SWITCH__`:** defeats TEST-03. The policy function MUST be host-compilable.
- **Editing one AppletFrame XML to add the banner:** there is no shared frame; each activity has its own. XML edit would only affect one screen.
- **Using `compare_exchange` for `cloudBusy` "just to be safe":** changes the read-then-write semantics; the current code does separate `if (cloudBusy) return; ... cloudBusy = true;` — a plain atomic load+store preserves this exactly (see CONC-01 note).
- **Removing `ensure_parent_dirs` before proving equivalence (D-05):** the two variants differ in subtle ways (see table below).
- **Forgetting `tests/Makefile` is not glob-based:** `fs_util.cpp` won't link into tests unless explicitly added.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Persistent global UI element | a from-scratch overlay drawing system | the existing `install_header_username` per-activity-helper pattern (`app_header.cpp`) | proven, already wired into all 14 activities, lowest risk |
| `std::atomic<bool>` guard | a mutex around `cloudBusy` | `std::atomic<bool>` (already `#include`d) | the `alive` member in the same class already uses `std::shared_ptr<std::atomic<bool>>` — atomic is the house style |
| Recursive copy / mkdir -p | a third fs implementation | the canonical `fs_util` form chosen below | the whole point of DEBT-01/02 is to STOP hand-rolling these |

**Key insight:** Every "build" in this phase is a *de-duplication*; the only genuinely new code is the tiny `tls_policy` seam and the banner helper.

## Runtime State Inventory

> This is a refactor phase, so this inventory applies. Scope is code-only — no stored data, no service config, no OS registrations are touched by these changes.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | **None** — no DB keys, collection names, or persisted IDs reference any symbol being renamed/moved. `ensure_parent_dirs`/`copy_tree`/`cloudBusy` are internal C++ symbols only. Verified by grep: matches are all in `source/`. | none |
| Live service config | **None** — no external service (API, n8n, etc.) references these C++ symbols. The thomaz-api is untouched this phase. | none |
| OS-registered state | **None** — no Task Scheduler / launchd / systemd / pm2 entries reference these symbols (they are in-process C++). | none |
| Secrets/env vars | **None** — no env var or secret name matches `ensure_parent_dirs`, `copy_tree`, `cloudBusy`, `tls_policy`, `ca_ok`. | none |
| Build artifacts | **CMake re-run required.** The app target is `file(GLOB_RECURSE source/*.cpp)` (`CMakeLists.txt:71`) — adding `fs_util.cpp` needs a **fresh CMake configure** (glob is evaluated at configure time, not build time). Existing `build_desktop/`/`build-desktop/` dirs are stale once a file is added; re-run `cmake` (not just `make`). The `tests/run` binary must be rebuilt after Makefile SRCS edit. | re-run `cmake`; rebuild tests |

**Canonical question answered:** After the source edits, the only "cached" state that won't auto-update is the **CMake glob result** — a stale build dir won't compile `fs_util.cpp` until reconfigured.

## Per-Call-Site Consolidation Facts (D-04 / D-05)

### `ensure_parent_dirs` — 4 definitions (all verified by read)

| File:line | Form | Loop bound | `mkdir` on full path? | Notes |
|-----------|------|-----------|----------------------|-------|
| `cheat_store.cpp:12` | substring-at-slash | `i=1 .. size()` | **No** (only parents, stops before last segment which is the file) | **CANONICAL (D-05).** `dir = path.substr(0,i)` at each `/`; guards `!dir.empty()`. Anonymous-namespace, `void` return. |
| `mod_download.cpp:12` | substring-at-slash | `i=1 .. size()` | No | identical algorithm to cheat_store, minus the `!dir.empty()` guard (harmless: `substr(0,i)` for `i>=1` is never empty). |
| `libarchive_extractor.cpp:13` | substring-at-slash | `i=1 .. size()` | No | identical to mod_download. Called as `ensure_parent_dirs(out_path + "/")` for dir entries (`:106`) — appends trailing slash so the dir itself gets created by the loop. |
| `theme_install.cpp:38` | **char-by-char accumulator** | `i=0 .. size()` | **Yes for trailing slash** | takes param named `file`; `acc.push_back(c); if (c=='/' && acc.size()>1) mkdir(acc)`. **Difference:** because it accumulates including the slash, a path ending in `/` creates that final dir too; and it starts at `i=0` so a leading `/` (absolute) is handled slightly differently. |

**D-05 equivalence analysis (the gate):**
- For a typical relative path like `romfs:/themes/a/b/c.bin` (no trailing slash), BOTH forms create exactly the parent dirs `romfs:`, `romfs:/themes`, `romfs:/themes/a`, `romfs:/themes/a/b` and NOT the final segment. **Equivalent.**
- **Edge case — trailing slash:** substring form (`cheat_store`) at `path="x/y/"`: creates `x`, `x/y` (the `/` at end is index `size()-1`, `substr(0,i)="x/y"` → creates `x/y`). char-by-char (`theme_install`) at `"x/y/"`: also creates `x`, `x/y`. **Equivalent for trailing slash** — both create `x/y`. (This matters because `libarchive_extractor` deliberately appends `/`.)
- **Edge case — leading `romfs:` prefix:** `romfs:/...` — the first `/` is after `romfs:`, so `substr(0, idx)` = `"romfs:"`; `mkdir("romfs:")` fails harmlessly (romfs read-only / already-mounted). Both forms attempt it; `mkdir` EEXIST/EROFS is ignored in both. **Equivalent.**
- **Edge case — empty segments (`a//b`):** substring form creates `a`, then at the second `/` creates `a/` (`substr` gives `"a/"`, mkdir of `"a/"` ≈ `"a"`, EEXIST). char-by-char creates `a`, then `a/` again. **Equivalent (both no-op on the duplicate).**
- **Conclusion:** the two are behaviorally equivalent for all paths used in this codebase. **D-05 gate test (`test_fs_util.cpp`) should assert** that the canonical `fs_util::ensure_parent_dirs` produces the same set of `mkdir` calls as the char-by-char form for `romfs:/themes/a/b/c` (and a trailing-slash case). The cleanest test: refactor both into testable form OR test observable dir creation under a temp root (the `test_cfw_paths.cpp` precedent uses `std::filesystem` + temp files — same approach works here).

**Canonical choice:** adopt the **substring-at-slash** form (`cheat_store.cpp:12`) including the `!dir.empty()` guard. Keep `void` return, `thomaz::` namespace (NOT anonymous — it's now shared). Callers that pass `out_path + "/"` (libarchive) keep working since trailing-slash is equivalent.

### `copy_tree` — 2 definitions + call-sites (all verified by read)

| File:line | Signature | Error reporting | `is_dir` check | mkdir dst |
|-----------|-----------|-----------------|----------------|-----------|
| `mod_store.cpp:42` | `bool copy_tree(src, dst, std::string* err)` | sets `*err` on failure | `is_dir()` via `stat` (helper at `:11`) | `::mkdir(dst_dir)` at top of fn | **CANONICAL — richer signature.** Declared in `mod_store.hpp:11`. Uses `copy_file()` helper (`:16`). |
| `save_service_switch.cpp:55` | `bool copy_tree(src, dst)` (no err) | returns bool only | `stat` + `S_ISDIR` inline | does NOT mkdir dst (comment says "both already exist"); mkdirs each subdir inline (`:71`) | also removes ghost dst file on open-fail (`:76`-ish) — a behavioral nicety the mod_store version lacks. |

**Consolidation note (DEBT-02):** the two `copy_tree` differ in signature (`err` param) AND in whether they mkdir the top-level dst. `save_service_switch` callers (`:206`, `:256`) rely on **both** dirs pre-existing and pass no `err`. Adopt the **mod_store signature** `bool copy_tree(src, dst, std::string* err)` as canonical (it's already the public one in `mod_store.hpp`). At the `save_service_switch` call sites, pass `nullptr` for `err` (or a local string that's ignored) — but VERIFY the top-level-mkdir difference doesn't double-create: mod_store's version does `::mkdir(dst_dir)` at the top, which is harmless EEXIST if the caller already created it. The ghost-file-removal-on-failure behavior in `save_service_switch` is an improvement; **fold it into the canonical `copy_tree`** so no behavior is lost. Call sites to update: `mod_actions.cpp:82` (already uses the 3-arg form — no change), `save_service_switch.cpp:71,206,256` (recursive self-call + 2 callers — switch to 3-arg, pass nullptr).

**DEBT-02 second location confirmed (research question 5):** `save_service_switch.cpp:55` `bool copy_tree(const std::string& src, const std::string& dst)` — **confirmed present**, resolving STATE.md's "unconfirmed" blocker. There is NO third copy (grep returned only these two definitions + the `mod_store.hpp` declaration).

## cloudBusy Atomicization (CONC-01)

**All access sites (verified `save_detail_activity.cpp`):** `:249` (read guard in `doUpload`), `:251`/`:296`/`:344` (set true), `:266`/`:275`/`:313`/`:322`/`:381` (set false), `:342` (read guard in `doDownload`). Declared `:47` of `.hpp` as `bool cloudBusy = false`.

**Threading reality:** Every write to `cloudBusy` is inside a `brls::sync([...])` closure (which posts back to the main thread) OR in a UI event handler (`doUpload`/`doDownload` entry, main thread). The two *reads* (`:249`, `:342`) are at the start of UI event handlers — main thread. **So it is de-facto main-thread-only today** (CONCERNS.md confirms this). CONC-01 makes it `std::atomic<bool>` to *document and enforce* the contract and to be race-safe if a future refactor (Phase 4's `runAsync`) moves a write off-thread.

**Recommended change:**
- `.hpp:47`: `std::atomic<bool> cloudBusy{false};` (header already `#include <atomic>` at line 4).
- Read sites `:249`, `:342`: `if (this->cloudBusy.load()) return;`
- Write sites: `this->cloudBusy.store(true);` / `this->cloudBusy.store(false);`
- **Do NOT use `compare_exchange`** — the current logic is "check, return-if-busy, then set busy" with other work in between; a CAS would change semantics. Plain load/store preserves exact behavior. (D-claude-discretion confirms planner picks the form; load/store is the minimal, semantics-preserving choice.)
- **CROSS-PHASE CONSTRAINT S2 (from CONTEXT):** ONLY atomicize `cloudBusy`. Do **NOT** touch the `alive` member (`.hpp` last line: `std::shared_ptr<std::atomic<bool>> alive`) — Phase 4's CONC-02 owns that. Leave `alive` exactly as-is.

## TLS Policy Seam (D-06 / TEST-03)

**Current code (verified `curl_tls.hpp:16-44`):** `inline void apply_curl_tls(CURL*)`. The `ca_ok` probe and the `if(ca_ok){verify on}else{verify off}` decision are **entirely inside `#ifdef __SWITCH__`**. The `#else` (desktop) branch always sets verify on. Host build (`tests/Makefile`, no `-D__SWITCH__` — verified `grep -c __SWITCH__ tests/Makefile` = 0) compiles ONLY the desktop branch, so the `ca_ok==false` path is unreachable from a host test today. **This is the core TEST-03 problem.**

**Fix (D-06):** extract a pure free function OUTSIDE the `#ifdef`:
```cpp
// curl_tls.hpp — host-compilable, no curl/Switch deps
namespace thomaz {
struct TlsPolicy { long verifypeer; long verifyhost; };
inline TlsPolicy tls_policy(bool ca_present) {
    return ca_present ? TlsPolicy{1L, 2L} : TlsPolicy{0L, 0L};
}
}
```
`apply_curl_tls` (inside `#ifdef __SWITCH__`) calls `auto p = tls_policy(ca_ok);` then `curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, p.verifypeer)` etc. The desktop `#else` calls `tls_policy(true)`. **TEST-03** (`tests/test_tls_policy.cpp`) asserts:
```cpp
#include "doctest.h"
#include "platform/curl_tls.hpp"   // host-safe: tls_policy is outside the #ifdef
using thomaz::tls_policy;
TEST_CASE("tls_policy disables verification when CA bundle is absent (fail-safe)") {
    auto p = tls_policy(false);
    CHECK(p.verifypeer == 0L);   // regression guard: silent-disable must be intentional
    CHECK(p.verifyhost == 0L);
}
TEST_CASE("tls_policy enables full verification when CA bundle is present") {
    auto p = tls_policy(true);
    CHECK(p.verifypeer == 1L);
    CHECK(p.verifyhost == 2L);
}
```
**Build note for TEST-03:** `curl_tls.hpp` includes `<curl/curl.h>`. To keep `test_tls_policy.cpp` host-buildable WITHOUT linking curl, EITHER (a) split `tls_policy` into a curl-free header `tls_policy.hpp` that `curl_tls.hpp` includes (cleanest — no curl include in the test), OR (b) ensure libcurl dev headers are present on the host (they are, since desktop build links `CURL::libcurl`). **Recommend (a):** a tiny `source/platform/tls_policy.hpp` with just the struct + function, included by both `curl_tls.hpp` and the test. This avoids pulling `<curl/curl.h>` into the doctest TU. The planner should confirm whether the host toolchain has curl headers; the split avoids the question entirely.

## Build Integration Facts (research questions 2, 3, 6)

### App build — CMake (`CMakeLists.txt`)
- **Q2:** `file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)` at **`CMakeLists.txt:71`** collects ALL `source/**/*.cpp`. **`fs_util.cpp` (and any new `.cpp` like `tls_banner.cpp`) is auto-included — NO CMakeLists edit needed.** BUT glob is evaluated at *configure* time → a fresh `cmake` configure is required (a stale `build_desktop/` won't see the new file from `make` alone).
- **Q6 (zero new warnings):** desktop flags come from `lib/borealis/library/cmake/toolchain.cmake:15-16`: `CMAKE_CXX_FLAGS_DEBUG/RELEASE` both append `-Wall` (NO `-Wextra`, NO `-Werror`). App target adds `-ffunction-sections -fdata-sections` (`CMakeLists.txt:106`) — no extra warning flags. So "zero new warnings" is **checkable by reading compiler stderr**, not enforced by `-Werror`. The verification step should `cmake --build build_desktop 2>&1 | grep -i warning` and assert no NEW lines vs a baseline. C++ standard for the app target is **C++20** (`CMakeLists.txt:82` `CXX_STANDARD 20`) — `fs_util.cpp` will compile as C++20.

### Test build — Makefile (`tests/Makefile`)
- **Q2/Q3:** `tests/Makefile` uses an **explicit SRCS list (NOT a glob over platform/)** — line `SRCS := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) ... ../source/platform/cheat_store.cpp ../source/platform/mods/mod_store.cpp ...`. The `*.cpp` wildcard covers `tests/*.cpp` (so a new `test_tls_policy.cpp` / `test_fs_util.cpp` is auto-picked **within tests/**), but each needed **platform** `.cpp` is listed by hand. **ACTION:** add `../source/platform/fs_util.cpp` to SRCS. Flags: `-std=c++17 -Wall -Wextra -I../lib/doctest -I../lib/json -I../source` (line `CXXFLAGS`). Note tests build at **C++17** while the app builds C++20 — `fs_util` must compile under **both** (avoid C++20-only features; the helpers use plain POSIX + std::string, so fine).
- **IMPORTANT linkage interaction:** `cheat_store.cpp` is ALREADY in the test SRCS list. Today `cheat_store.cpp` contains an anonymous-namespace `ensure_parent_dirs`. After consolidation, `cheat_store.cpp` will `#include "platform/fs_util.hpp"` and call `fs_util::ensure_parent_dirs`. The test build links `cheat_store.cpp` — so **`fs_util.cpp` MUST be added to SRCS or the test build breaks with an undefined reference** (`thomaz::ensure_parent_dirs`). Same for `mod_store.cpp` (also in SRCS, also a `copy_tree` site). This is the single most likely build break — flag it prominently.
- **Run:** `cd tests && make test` builds `run` and executes it; `./tests/run` runs the pre-built binary. `make clean` removes `run`. Doctest entry is `tests/test_main.cpp` (`DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`). New test files need NO registration beyond being in `tests/*.cpp` (caught by `$(wildcard *.cpp)`) — doctest auto-collects `TEST_CASE`s at link time.

## Common Pitfalls

### Pitfall 1: tests/Makefile undefined-reference after consolidation
**What goes wrong:** Move `ensure_parent_dirs`/`copy_tree` to `fs_util.cpp`, forget to add it to `tests/Makefile` SRCS → `cheat_store.cpp` and `mod_store.cpp` (already in SRCS) reference `thomaz::ensure_parent_dirs`/`copy_tree` → linker error in `make test`.
**How to avoid:** Add `../source/platform/fs_util.cpp` to SRCS in the SAME change that creates fs_util.
**Warning sign:** `undefined reference to thomaz::ensure_parent_dirs(...)` at `make test`.

### Pitfall 2: Stale CMake build dir doesn't compile fs_util.cpp
**What goes wrong:** `make` in an existing `build_desktop/` won't pick up the new file (GLOB cached at configure time) → app still links the OLD inline copies (or fails).
**How to avoid:** Re-run `cmake -DUSE_SDL2=ON ...` (fresh configure) before `make`. The verification command must reconfigure.
**Warning sign:** the new file never appears in build output; old behavior persists.

### Pitfall 3: tls_policy left curl-dependent breaks host test isolation
**What goes wrong:** Test TU includes `curl_tls.hpp` → pulls `<curl/curl.h>` → if curl headers absent on host or test shouldn't link curl, compile fails.
**How to avoid:** Split `tls_policy` into curl-free `source/platform/tls_policy.hpp`; include from both.
**Warning sign:** `fatal error: curl/curl.h: No such file` in `make test`.

### Pitfall 4: Namespace collision when un-anonymizing helpers
**What goes wrong:** The 4 `ensure_parent_dirs` are in anonymous namespaces (file-local). The consolidated one is `thomaz::ensure_parent_dirs` (external linkage). If any file still defines its own AND includes fs_util, you get a redefinition / ambiguity.
**How to avoid:** Delete EVERY local copy in the same change; replace each call with the `fs_util` include + qualified call. Grep `ensure_parent_dirs`/`copy_tree` after editing — only `fs_util.{hpp,cpp}` should define them.
**Warning sign:** `redefinition of` or `call is ambiguous`.

### Pitfall 5: C++17 vs C++20 mismatch
**What goes wrong:** `fs_util.cpp` uses a C++20 feature → app (C++20) compiles, tests (C++17) fail.
**How to avoid:** Keep `fs_util` to POSIX + `std::string` (the existing helpers already are). No `std::format`, no `<=>`, no `std::span`.
**Warning sign:** test build error referencing a C++20 feature.

### Pitfall 6: Banner shows on desktop or never clears
**What goes wrong:** Insecure flag set unconditionally, or banner logic runs on desktop where TLS always verifies.
**How to avoid:** Set `tls_insecure_flag()` ONLY in the Switch `ca_ok==false` branch (inside `#ifdef __SWITCH__`). Banner helper checks `tls_is_insecure()` — false on desktop, so no banner on host. The flag is a one-way latch (set once, never cleared) which matches the `static ca_ok` "computed once" semantics noted in CONTEXT.

## Code Examples (verified from this repo)

### Adding a test file (precedent: tests/test_cfw_paths.cpp)
```cpp
// Source: tests/test_cfw_paths.cpp (verified) — pattern for TEST-03
#include "doctest.h"
#include "platform/themes/cfw_paths.hpp"
using namespace thomaz;
TEST_CASE("...descriptive sentence...") {
    CHECK(some_pure_fn(input) == expected);
    CHECK_FALSE(some_pure_fn("").has_value());
}
```

### Header-injection helper (precedent: source/app/app_header.cpp)
```cpp
// Source: source/app/app_header.cpp (verified) — pattern for install_tls_warning_banner
void install_tls_warning_banner(brls::Activity* activity) {
    if (!activity) return;
    if (!thomaz::tls_is_insecure()) return;            // desktop / normal: no-op
    auto* header = dynamic_cast<brls::Box*>(
        activity->getView("brls/applet_frame/header")); // or hint_box for a chip
    if (!header) return;
    auto* lbl = new brls::Label();
    lbl->setText("thomaz/tls/insecure_warning"_i18n);   // new i18n key
    lbl->setTextColor(nvgRGB(0xFF, 0x55, 0x55));
    header->addView(lbl, 0);                            // index 0 = leftmost / topmost
}
```

### i18n key addition (precedent: resources/i18n/en-US/thomaz.json)
The file is a nested JSON object (verified). Add under a new `"tls"` object, e.g. `"tls": { "insecure_warning": "Certificate verification unavailable — connection is not secure" }`, referenced as `"thomaz/tls/insecure_warning"_i18n`. Mirror the key (English text or translation) into the other locales under `resources/i18n/{fr,pt-BR,ru,zh-Hans}/thomaz.json` to avoid missing-key fallbacks.

## State of the Art

| Old Approach | Current Approach | Why |
|--------------|------------------|-----|
| 4× anonymous-namespace `ensure_parent_dirs` | single `thomaz::fs_util::ensure_parent_dirs` | DEBT-01 |
| 2× `copy_tree` (divergent signatures/error handling) | single canonical `copy_tree(src,dst,err*)` | DEBT-02 |
| `bool cloudBusy` (undocumented main-thread invariant) | `std::atomic<bool> cloudBusy` (enforced contract) | CONC-01 |
| TLS fail-safe silent (log-comment only) | persistent on-screen banner + host-tested policy seam | SEC-03 + TEST-03 |

**Deprecated/outdated:** none — no library version churn in this phase.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The two `ensure_parent_dirs` variants are behaviorally equivalent for all paths used in this codebase | D-05 analysis | LOW — D-05 mandates a gate test that *proves* this before removal; if the test fails, the canonical form is adjusted. The analysis is from direct code reading, not external knowledge. |
| A2 | All `cloudBusy` accesses are main-thread (so load/store suffices, no CAS) | CONC-01 | LOW — verified every site is inside `brls::sync` or a UI handler; CONCERNS.md independently states the same. A future off-thread write is Phase 4's concern. |
| A3 | The host toolchain has libcurl dev headers (so `curl_tls.hpp` includes compile in tests) — mitigated by recommending a curl-free `tls_policy.hpp` split | TEST-03 build note | LOW — the split removes the dependency entirely; if headers are present the split is still the cleaner option. |
| A4 | Adding the banner via per-activity helper satisfies D-02 "global/persistent" | Pattern 1 | MEDIUM — D-02 says "fixed banner in the main Application shell." There is no single shell (each activity owns its AppletFrame), so "global" is necessarily realized as "injected into every activity's frame." The planner/user should confirm the per-activity-helper realization is acceptable vs. investing in a custom always-on overlay (Pattern 2). This is the one genuine design choice to surface. |

## Open Questions

1. **Banner realization: per-activity helper (Pattern 1) vs custom overlay (Pattern 2)?**
   - What we know: there is no shared root frame; `install_header_username` proves the per-activity-helper pattern works and is already universal. Borealis has no public custom-overlay API.
   - What's unclear: whether D-02's "main Application UI shell" intends a literal single shell (which doesn't exist here) or "visible on every screen" (which the helper achieves).
   - Recommendation: go with Pattern 1 (per-activity helper, mirroring `app_header.cpp`); it is lowest-risk and matches D-02's *intent* (maximum visibility, every screen). Flag A4 for user confirmation in discuss/planning.

2. **Banner slot: `header` Box vs `hint_box` chip vs a new top strip?**
   - What we know: `hint_box` already holds the username; `header` is the full top bar.
   - Recommendation: inject a high-contrast Label/Box into `brls/applet_frame/header` at index 0 (leftmost) or as a colored chip; planner picks exact styling (Claude's discretion per CONTEXT D-36-37).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| g++ (C++17) | tests/Makefile host build | assume ✓ (37 existing tests build this way) | host gcc | — |
| cmake + SDL2 desktop toolchain | `-DUSE_SDL2=ON` app build | ✓ (existing `build_desktop/` has `USE_SDL2:BOOL=ON`) | — | — |
| libcurl dev headers (host) | only if `curl_tls.hpp` is included by the test | ✓ desktop links `CURL::libcurl` | system | split `tls_policy.hpp` removes the need (recommended) |
| doctest header | host tests | ✓ vendored `lib/doctest/doctest.h` | — | — |

**Missing dependencies with no fallback:** none identified — this is an internal-refactor phase against an already-building tree.

## Project Constraints (from existing docs — no CLAUDE.md present)

- **No CLAUDE.md** in the repo (verified) and **no `.claude/skills/` or `.agents/skills/`** directories. Conventions come from `.planning/codebase/CONVENTIONS.md`:
  - C++ files `snake_case.{cpp,hpp}`; test files `test_<module>.cpp`.
  - Free functions `snake_case`; types/structs `PascalCase`; all project code under `thomaz::` (sub-namespaces allowed). Anonymous namespaces for file-local helpers — but the consolidated `fs_util` helpers must be in `thomaz::` (external linkage), NOT anonymous.
  - `#pragma once` for headers (no include guards).
  - C++: no exceptions; structs-with-`.ok()`/error-enum for error reporting. (`copy_tree` keeps its `bool` + `*err` convention.)
  - `commit_docs: true` (`.planning/config.json`) → RESEARCH.md should be committed.

## Validation Architecture

> SKIPPED — `.planning/config.json` sets `workflow.nyquist_validation: false`. (TEST-03's host doctest is still required by the phase requirements and is documented under the TLS Policy Seam section above; the formal Nyquist Validation Architecture section is omitted per config.)

## Security Domain

> `security_enforcement` is not set in config → treated as enabled. This phase is itself a security hardening (SEC-03).

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | not touched this phase |
| V3 Session Management | no | not touched |
| V4 Access Control | no | not touched |
| V5 Input Validation | no | filesystem helpers operate on internally-constructed paths; zip-slip is already guarded by `core::is_safe_archive_path` (verified `libarchive_extractor.cpp`) — unchanged here |
| V6 Cryptography | **yes** | **V6/V9 Communications: TLS.** The whole point of SEC-03/TEST-03: never silently disable cert verification without (a) a visible warning and (b) a regression test that fails if verification is disabled unintentionally. The fail-safe *behavior* is an accepted, documented trade-off (REQUIREMENTS: keeps self-updater alive); the controls added are *detection* (banner) + *regression guard* (TEST-03). |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Silent TLS downgrade (MITM on-path when CA bundle missing) | Spoofing / Information Disclosure | persistent on-screen warning (SEC-03) + host regression test asserting `tls_policy(false)` is an *intentional* `{0,0}` (TEST-03). Note: the downgrade itself is retained by explicit product decision — only made visible + test-guarded. |
| Data race on `cloudBusy` enabling double upload/download | Tampering (state corruption) | `std::atomic<bool>` (CONC-01) |
| Path traversal via fs helpers | Tampering | out of scope — paths are internally constructed; archive paths already guarded upstream (`is_safe_archive_path`) |

## Sources

### Primary (HIGH confidence — direct repo reads)
- `source/platform/curl_tls.hpp` (lines 16-44) — apply_curl_tls / ca_ok / #ifdef structure
- `source/platform/cheat_store.cpp:12`, `mod_download.cpp:12`, `libarchive_extractor.cpp:13`, `themes/theme_install.cpp:38` — the 4 ensure_parent_dirs
- `source/platform/mods/mod_store.cpp:42` + `mod_store.hpp:11`, `save_service_switch.cpp:55` — the 2 copy_tree
- `source/app/save_detail_activity.{hpp:47,cpp:249-381}` — cloudBusy sites
- `source/app/app_header.{hpp,cpp}` — install_header_username banner precedent
- `source/main.cpp` — Application init / activity push (no shared shell)
- `CMakeLists.txt:71,82,106` — GLOB_RECURSE, C++20, compile options
- `tests/Makefile` — explicit SRCS list, C++17, run targets
- `lib/borealis/library/include/borealis/views/applet_frame.hpp:54-89` — header BRLS_BIND slots
- `lib/borealis/library/lib/core/{application.cpp:770-794,1157, notification_manager.cpp}` — render loop + overlay precedent
- `lib/borealis/library/cmake/toolchain.cmake:15-16` — desktop -Wall flags (no -Werror)
- `.planning/config.json` — nyquist_validation false, commit_docs true
- `resources/i18n/en-US/thomaz.json` — i18n nesting; `resources/xml/activity/*.xml` — per-activity AppletFrame

### Secondary / Tertiary
- None — no external sources needed; everything verified in-tree.

## Metadata

**Confidence breakdown:**
- fs_util consolidation (signatures, diffs, D-05 equivalence): HIGH — all 6 definitions read line-by-line.
- Build integration (CMake GLOB, tests/Makefile manual-add): HIGH — verified the exact lines; the undefined-reference risk is concrete.
- TLS seam (D-06/TEST-03): HIGH — current #ifdef structure read directly; the extraction is mechanical.
- cloudBusy atomicization: HIGH — all 11 sites enumerated.
- Banner injection: HIGH on the *mechanism* (app_header precedent verified), MEDIUM on *which* realization satisfies D-02's wording (flagged A4 / Open Q1).

**Research date:** 2026-06-05
**Valid until:** 2026-07-05 (stable — internal refactor, no external dependency churn; only invalidated by edits to the named files)
