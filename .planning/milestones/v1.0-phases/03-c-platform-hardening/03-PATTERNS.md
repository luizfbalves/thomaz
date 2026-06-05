# Phase 3: C++ Platform Hardening - Pattern Map

**Mapped:** 2026-06-05
**Files analyzed:** 16 (3 new + 13 modified)
**Analogs found:** 16 / 16 (all in-tree; this is a de-duplication/refactor phase)

> No external libraries. Every new artifact mirrors an existing in-repo pattern. Conventions source: `.planning/codebase/CONVENTIONS.md` (no CLAUDE.md, no skills dirs). House style: `snake_case.{cpp,hpp}`, `#pragma once`, `thomaz::` namespace, no exceptions, `bool` + `std::string* err` for error reporting.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `source/platform/fs_util.hpp` (NEW) | utility (header) | file-I/O | `source/platform/mods/mod_store.hpp` | exact |
| `source/platform/fs_util.cpp` (NEW) | utility | file-I/O | `source/platform/mods/mod_store.cpp` + `cheat_store.cpp` | exact |
| `source/platform/tls_policy.hpp` (NEW) | utility (pure policy) | transform | `source/platform/themes/cfw_paths.hpp` (pure, host-compilable) | role-match |
| `source/app/tls_banner.{hpp,cpp}` (NEW) | provider (UI helper) | event-driven (per-activity) | `source/app/app_header.{hpp,cpp}` | exact |
| `tests/test_tls_policy.cpp` (NEW, TEST-03) | test | transform | `tests/test_cfw_paths.cpp` | exact |
| `tests/test_fs_util.cpp` (NEW, D-05 gate) | test | file-I/O | `tests/test_cfw_paths.cpp` (uses `std::filesystem` + temp) | exact |
| `source/platform/curl_tls.hpp` (MOD) | config (TLS opts) | request-response | self (extract `tls_policy`, set insecure flag) | self |
| `source/platform/cheat_store.cpp` (MOD) | service | file-I/O | self (drop local `ensure_parent_dirs`) | self |
| `source/platform/themes/theme_install.cpp` (MOD) | service | file-I/O | self (drop char-by-char variant) | self |
| `source/platform/mods/libarchive_extractor.cpp` (MOD) | service | file-I/O | self (drop local copy) | self |
| `source/platform/mods/mod_download.cpp` (MOD) | service | file-I/O | self (drop local copy) | self |
| `source/platform/mods/mod_store.{cpp,hpp}` (MOD) | service | file-I/O | self (move `copy_tree` out to fs_util) | self |
| `source/platform/mods/mod_actions.cpp` (MOD) | service | file-I/O | self (call fs_util; already 3-arg) | self |
| `source/platform/save_service_switch.cpp` (MOD) | service | file-I/O | self (drop 2-arg `copy_tree`, call fs_util 3-arg) | self |
| `source/app/save_detail_activity.{hpp,cpp}` (MOD) | component (activity) | event-driven | self (`cloudBusy` -> `std::atomic<bool>`) | self |
| `tests/Makefile` (MOD) | config (build) | batch | self (add `fs_util.cpp` to SRCS) | self |
| `CMakeLists.txt` (verify only) | config (build) | batch | self (GLOB auto-picks `fs_util.cpp`; re-configure) | self |

## Pattern Assignments

### `source/platform/fs_util.hpp` + `fs_util.cpp` (NEW — utility, file-I/O)

**Analog:** `source/platform/mods/mod_store.{hpp,cpp}` (header+cpp split, namespace, copy_tree signature) and `source/platform/cheat_store.cpp` (canonical `ensure_parent_dirs`).

**Header layout to copy** — from `mod_store.hpp:1-26` (`#pragma once`, includes, `thomaz::` namespace, doc-comment-above-declaration convention):
```cpp
#pragma once
#include <optional>
#include <string>
#include <vector>

namespace thomaz {
// ... declarations with doc comments ...
} // namespace thomaz
```

**`ensure_parent_dirs` canonical form (D-05)** — copy verbatim from `cheat_store.cpp:11-21` (substring-at-slash, `i=1..size()`, `!dir.empty()` guard, `void` return). Promote from anonymous namespace to `thomaz::` (external linkage):
```cpp
// mkdir -p for the directory portion of `path` (everything before the last '/').
void ensure_parent_dirs(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i) {
        if (path[i] != '/') continue;
        std::string dir = path.substr(0, i);
        if (!dir.empty())
            ::mkdir(dir.c_str(), 0777); // ignore EEXIST and friends
    }
}
```
> REJECTED variant: `theme_install.cpp:37-44` char-by-char accumulator. RESEARCH D-05 proves them equivalent for all repo paths; keep cheat_store's form.

**`copy_tree` canonical form (DEBT-02)** — copy from `mod_store.cpp:42-74` (the 3-arg `std::string* err` signature is canonical, already declared in `mod_store.hpp:11`). Pull the `is_dir` (`mod_store.cpp:12-15`) and `copy_file` (`mod_store.cpp:17-38`) helpers along with it:
```cpp
bool copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err) {
    ::mkdir(dst_dir.c_str(), 0777); // ignore EEXIST  (harmless if caller pre-created)
    DIR* d = ::opendir(src_dir.c_str());
    if (!d) { if (err) *err = "cannot open " + src_dir; return false; }
    bool ok = true;
    while (struct dirent* e = ::readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string src = src_dir + "/" + name, dst = dst_dir + "/" + name;
        if (is_dir(src)) { if (!copy_tree(src, dst, err)) { ok = false; break; } }
        else if (!copy_file(src, dst)) { if (err) *err = "cannot copy " + src; ok = false; break; }
    }
    ::closedir(d);
    return ok;
}
```
> DEBT-02 note: fold `save_service_switch.cpp`'s ghost-file-removal-on-open-fail nicety into `copy_file` so no behavior is lost (RESEARCH line 205). `save_service_switch` 2-arg callers pass `nullptr` for `err`.

**Includes for `.cpp`** — from `cheat_store.cpp:1-5` / `mod_store.cpp:1-6`: `<cstdio> <dirent.h> <sys/stat.h> <unistd.h>` + `#include "platform/fs_util.hpp"`.

**Constraint:** must compile under BOTH C++20 (app) and C++17 (tests) — POSIX + `std::string` only, no `std::format`/`<=>`/`std::span` (RESEARCH Pitfall 5).

---

### `source/platform/tls_policy.hpp` (NEW — pure policy, transform)

**Analog:** `source/platform/themes/cfw_paths.hpp` — pure, host-compilable platform header (no Switch/curl deps), `thomaz::` namespace, `#pragma once`.

**Pattern to write** (RESEARCH lines 229-235; split OUT of `curl_tls.hpp` so the doctest TU never pulls `<curl/curl.h>` — Pitfall 3):
```cpp
#pragma once
namespace thomaz {
struct TlsPolicy { long verifypeer; long verifyhost; };
inline TlsPolicy tls_policy(bool ca_present) {
    return ca_present ? TlsPolicy{1L, 2L} : TlsPolicy{0L, 0L};
}
} // namespace thomaz
```
> No curl include. Lives outside any `#ifdef __SWITCH__` — that is the whole point of the TEST-03 seam (D-06).

---

### `source/platform/curl_tls.hpp` (MODIFIED — config, request-response)

**Analog:** self. Current code `curl_tls.hpp:16-40` (read this exact range).

**Change 1 — consume `tls_policy`:** `#include "platform/tls_policy.hpp"`. Inside `#ifdef __SWITCH__`, replace the literal `1L/2L` and `0L/0L` `curl_easy_setopt` pairs (lines 27-35) with `auto p = tls_policy(ca_ok);` then `setopt(...VERIFYPEER, p.verifypeer)` / `(...VERIFYHOST, p.verifyhost)`. Desktop `#else` (lines 36-39) -> `auto p = tls_policy(true);`.

**Change 2 — insecure flag (Pattern 3, RESEARCH 134-145):** add host-safe latch OUTSIDE the `#ifdef`:
```cpp
inline bool& tls_insecure_flag() { static bool f = false; return f; }
inline bool tls_is_insecure() { return tls_insecure_flag(); }
```
In the Switch `ca_ok == false` else-branch (current line 31-35) set `tls_insecure_flag() = true;`. One-way latch — never cleared (mirrors `static ca_ok` once-per-process semantics). Desktop never sets it -> banner never shows on host (Pitfall 6).

---

### `source/app/tls_banner.{hpp,cpp}` (NEW — UI provider, event-driven)

**Analog:** `source/app/app_header.{hpp,cpp}` — `install_header_username(brls::Activity*)`, verified `app_header.cpp:1-27`.

**Pattern to copy verbatim (structure)** — from `app_header.cpp:10-27` (null-guard activity, `dynamic_cast<brls::Box*>(activity->getView(...))`, null-guard view, `new brls::Label`, `setTextColor(nvgRGB(...))`, `addView`):
```cpp
#include "app/app_header.hpp"   // mirror include style
#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include "platform/curl_tls.hpp"   // for tls_is_insecure()  (or tls_policy.hpp + flag accessor)
using namespace brls::literals;

namespace thomaz {
void install_tls_warning_banner(brls::Activity* activity) {
    if (!activity) return;
    if (!thomaz::tls_is_insecure()) return;             // desktop/normal: no-op
    auto* header = dynamic_cast<brls::Box*>(
        activity->getView("brls/applet_frame/header"));  // full top bar (vs hint_box chip)
    if (!header) return;
    auto* lbl = new brls::Label();
    lbl->setText("thomaz/tls/insecure_warning"_i18n);
    lbl->setTextColor(nvgRGB(0xFF, 0x55, 0x55));        // high-contrast red, vs username 0x9277FF
    header->addView(lbl, 0);                             // index 0 = leftmost
}
} // namespace thomaz
```
**Wiring:** call `install_tls_warning_banner(this)` in every activity's `onContentAvailable()`, exactly where each already calls `install_header_username(this)` (e.g. `home_activity.cpp:31`, ~14 activities — RESEARCH line 122). Header slot IDs available: `brls/applet_frame/header`, `.../hint_box`, `.../title_label`, `.../footer` (`applet_frame.hpp:84-89`).
**i18n:** add `"tls": { "insecure_warning": "..." }` to `resources/i18n/en-US/thomaz.json` and mirror to `fr,pt-BR,ru,zh-Hans` (RESEARCH line 327).
> A4 / Open Q1 (MEDIUM): per-activity helper realizes D-02 "global" as "injected on every screen". Confirmed acceptable per CONTEXT D-02a. A new future activity that forgets the call lacks the banner — document the requirement.

---

### `source/app/save_detail_activity.{hpp,cpp}` (MODIFIED — component, event-driven)

**Analog:** self. The sibling `alive` member already uses `std::atomic` — atomic is the house style (RESEARCH line 159). `<atomic>` already included at `.hpp:4`.

- `.hpp:47`: `bool cloudBusy = false;` -> `std::atomic<bool> cloudBusy{false};`
- Read guards `cpp:249`, `:342`: `if (this->cloudBusy.load()) return;`
- Write sites `cpp:251,296,344` (true) and `:266,275,313,322,381` (false): `.store(true)` / `.store(false)`
- **Do NOT** use `compare_exchange` (changes check-then-set semantics — Anti-Pattern, RESEARCH 150).
- **Cross-phase S2:** touch ONLY `cloudBusy`. Leave the `alive` member untouched (Phase 4 / CONC-02 owns it).

---

### `tests/test_tls_policy.cpp` (NEW — TEST-03, transform) and `tests/test_fs_util.cpp` (NEW — D-05 gate, file-I/O)

**Analog:** `tests/test_cfw_paths.cpp:1-30` — `#include "doctest.h"`, then the platform header under test, `using namespace thomaz;`, `TEST_CASE("descriptive sentence")` with `CHECK`/`CHECK_FALSE`. For fs tests, `test_cfw_paths.cpp:3-4,28-30` shows the `std::filesystem` + temp-dir precedent.

**`test_tls_policy.cpp`** (RESEARCH 237-251):
```cpp
#include "doctest.h"
#include "platform/tls_policy.hpp"   // curl-free header -> no libcurl link needed
using thomaz::tls_policy;
TEST_CASE("tls_policy disables verification when CA bundle is absent (fail-safe)") {
    auto p = tls_policy(false);
    CHECK(p.verifypeer == 0L);
    CHECK(p.verifyhost == 0L);
}
TEST_CASE("tls_policy enables full verification when CA bundle is present") {
    auto p = tls_policy(true);
    CHECK(p.verifypeer == 1L);
    CHECK(p.verifyhost == 2L);
}
```
**`test_fs_util.cpp`** (D-05): create a temp tree with `std::filesystem`, call `fs_util::ensure_parent_dirs("<tmp>/themes/a/b/c.bin")` and a trailing-slash case, assert exactly the parent dirs exist and the final segment does NOT. Both new files are auto-collected by `$(wildcard *.cpp)` — no registration beyond existing in `tests/`.

---

### `tests/Makefile` (MODIFIED — build config) + `CMakeLists.txt` (verify only)

**Analog:** self, `tests/Makefile:3` (explicit SRCS list, NOT a platform glob).

- **REQUIRED:** append `../source/platform/fs_util.cpp` to the `SRCS :=` list (line 3). `cheat_store.cpp` and `mod_store.cpp` are ALREADY in SRCS and will reference `thomaz::ensure_parent_dirs` / `thomaz::copy_tree` after consolidation — without `fs_util.cpp` the test link fails with `undefined reference` (Pitfall 1 — the single most likely break).
- `tls_policy.hpp` is header-only; no SRCS add unless the policy lands in a `.cpp` (it does not — keep it `inline` in the header).
- Tests are C++17 (`Makefile:2`); app is C++20 (`CMakeLists.txt:82`). `fs_util` must compile under both.
- **CMakeLists.txt:** `file(GLOB_RECURSE ... source/*.cpp)` at `:71` auto-picks `fs_util.cpp` and `tls_banner.cpp` — NO edit. BUT glob is configure-time: re-run `cmake -DUSE_SDL2=ON ...` (fresh configure), not just `make` (Pitfall 2).

## Shared Patterns

### Namespace + linkage (consolidation)
**Source:** `.planning/codebase/CONVENTIONS.md`; `cheat_store.cpp:9` (anon) vs `mod_store.cpp:42` (external).
**Apply to:** `fs_util.{hpp,cpp}`. Consolidated helpers move from anonymous namespace -> `thomaz::` (external linkage). Delete EVERY local copy in the SAME change and replace with `#include "platform/fs_util.hpp"` + qualified call; grep `ensure_parent_dirs`/`copy_tree` after — only `fs_util.{hpp,cpp}` should define them (Pitfall 4: redefinition/ambiguity if any local copy survives).

### Header-injection UI helper
**Source:** `source/app/app_header.cpp:10-27`.
**Apply to:** `tls_banner.cpp` and its call-site in all ~14 activities' `onContentAvailable` (paired with the existing `install_header_username(this)`).

### Pure host-compilable seam (outside `#ifdef __SWITCH__`)
**Source:** `cfw_paths.hpp` (pure) vs `curl_tls.hpp:17-39` (`#ifdef`-gated).
**Apply to:** `tls_policy.hpp`. Decision logic must sit outside the Switch guard so the host doctest exercises real logic, not a mock (D-06 / Anti-Pattern RESEARCH 148).

### Error reporting convention
**Source:** `mod_store.{hpp:11,cpp:42-49}` — `bool` return + `std::string* err`.
**Apply to:** `fs_util::copy_tree`. No exceptions (CONVENTIONS).

## No Analog Found

None. Every file maps to an in-tree precedent (refactor/de-dup phase).

## Metadata

**Analog search scope:** `source/platform/`, `source/platform/mods/`, `source/platform/themes/`, `source/app/`, `tests/`, `CMakeLists.txt`
**Files read for excerpts:** `app_header.cpp`, `curl_tls.hpp`, `cheat_store.cpp`, `mod_store.cpp`, `mod_store.hpp`, `theme_install.cpp`, `save_detail_activity.hpp`, `test_cfw_paths.cpp`, `tests/Makefile`
**Pattern extraction date:** 2026-06-05
