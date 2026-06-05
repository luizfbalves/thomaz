# Phase 2: Full Extraction Engine - Pattern Map

**Mapped:** 2026-06-05
**Files analyzed:** 7 (4 modified source, 1 modified lib, 2 validation primitives reused)
**Analogs found:** 7 / 7

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `source/platform/themes/firmware_extract_switch.cpp` | platform service (real) | file-I/O + transform | itself (Phase 1) — generalize single-target to per-title loop | exact |
| `source/platform/themes/firmware_extract_fake.cpp` | platform service (no-op) | — | itself (Phase 1) — API signature may widen; stub body unchanged | exact |
| `source/platform/themes/firmware_extract.hpp` | interface header | request-response | itself (Phase 1) — add `ExtractAllResult` or widen return if multi-target | exact |
| `source/platform/themes/nca_extract_switch.cpp` | NCA facade (real) | transform (file-I/O) | itself (Phase 1) — `nca_romfs_filter` widen from single name to `/lyt/` dir glob | exact |
| `source/platform/themes/key_loader_switch.cpp` | privileged service | request-response | itself (Phase 1) — `resolve_nca_path` already accepts any title-ID string | exact |
| `source/platform/themes/cfw_paths.cpp` | path/mapping util | CRUD | itself (Phase 1) — add `common` entry to `target_map()`, verify `base_present_for` | exact |
| `lib/switchthemes/Common.{hpp,cpp}` | vendored mapping table | — | itself — `ThemeTargetInfo` tables are the authoritative title-ID/szs source to reconcile against `target_map()` | reference |

## Pattern Assignments

---

### `source/platform/themes/firmware_extract_switch.cpp` (platform service, real)

**Analog:** itself at Phase 1 — the single-target `extract_base_layout()` body is the inner loop body for Phase 2.

**Whole-file guard — unchanged** (lines 1–6, 169):
```cpp
#include "platform/themes/firmware_extract.hpp"
#include "platform/themes/key_loader_switch.hpp"
#include "platform/themes/nca_extract_switch.hpp"
#include "platform/themes/cfw_paths.hpp"

#ifdef __SWITCH__
...
#endif // __SWITCH__
```
All libnx/hactool headers remain inside the guard. Desktop TU stays empty.

**New entry-point shape — replace single `extract_base_layout(target)` with a multi-target driver.** The Phase 1 body becomes the per-`(title_id, szs_name)` inner loop. Decision D-02/D-02a split:

```cpp
// Systemic failures → hard abort (D-02a): applet gate + BIS/SPL session.
// Per-part failures → collect and continue (D-02): missing szs or bad decrypt.

struct ExtractAllResult {
    bool ok;                       // false only on systemic abort
    std::string systemic_error;    // non-empty on hard abort
    std::vector<std::string> failed_parts;   // per-part warnings
    std::vector<std::string> written_parts;  // successfully written canonical paths
};
```

**Session lifecycle — mount BIS once, reuse across all three titles** (D-02a: if BIS/SPL open fails, abort everything):
```cpp
// (1) Applet gate (from firmware_extract_switch.cpp:64-68)
if (appletGetAppletType() != AppletType_Application) { ... hard abort ... }

// (2) Open session ONCE before the title loop (from key_loader_switch.cpp:81-120)
KeyDerivationOutput kdo = open_privileged_session_and_derive_key();
if (!kdo.error.empty()) { close_privileged_session(); /* hard abort */ }

// (3) Per-title loop — call resolve_nca_path(title_id) for each title
// (4) Per-part loop — for each szs extracted from that title's NCA
// (5) Validate (is_valid_szs) + write (ensure_parent_dirs + write_file)
//     On per-part failure: push to failed_parts, continue
// (6) close_privileged_session() after ALL titles processed (or on hard abort)
```

**Inner loop — per-title/per-part (from existing lines 117-164):**
```cpp
// resolve_nca_path already handles any title_id string (key_loader_switch.cpp:213)
std::string nca_path = resolve_nca_path(title_id);
if (nca_path.empty()) { failed_parts.push_back(...); continue; }

// filter_list = {"/lyt/"} directory prefix OR enumerate all /lyt/*.szs (D-01)
// — builder's call per context; pass the full /lyt/ prefix to nca_romfs_filter
std::vector<std::string> filter_list = {"/lyt/"};  // widened from single filename

NcaExtractResult nca_res = extract_szs_from_nca(nca_path, kdo.header_key, filter_list);
if (!nca_res.error.empty()) { failed_parts.push_back(...); continue; }

// Per-file iteration over nca_res.files (was single lookup at line 141-148)
for (auto& [romfs_key, szs_buf] : nca_res.files) {
    if (!is_valid_szs(szs_buf)) { failed_parts.push_back(romfs_key + ": invalid szs"); continue; }
    std::string szs_name = /* basename of romfs_key */;
    std::string out_path = base_layout_dir() + "/" + szs_name;
    ensure_parent_dirs(out_path);
    if (!write_file(out_path, szs_buf)) { failed_parts.push_back(out_path + ": write failed"); }
    else { written_parts.push_back(out_path); }
}
```

**Validation — unchanged** (`is_valid_szs` at lines 22-29, SARC/Yaz0 magic check). The D-04 requirement to Yaz0-decompress + SARC-unpack is a stronger check; see "Validation Primitives" shared pattern below for upgrade path using `lib/switchthemes/SarcLib`.

**Firmware version capture — unchanged** (lines 88-97, `setsysGetFirmwareVersion`). Run once before the loop; applies to all titles from the same extraction run.

---

### `source/platform/themes/firmware_extract_fake.cpp` (platform service, no-op)

**Analog:** itself at Phase 1 — lines 1-13 are the complete body.

**Pattern to copy verbatim** (firmware_extract_fake.cpp:1-13):
```cpp
#include "platform/themes/firmware_extract.hpp"

#ifndef __SWITCH__

namespace thomaz {

ExtractResult extract_base_layout(const std::string& /*target*/) {
    return {false, "Firmware extraction is only available on Switch."};
}

} // namespace thomaz

#endif // !__SWITCH__
```
If the entry-point signature changes to `ExtractAllResult extract_all_base_layouts()` the stub returns `{false, "...", {}, {}}` — still zero libnx symbols. No other changes.

---

### `source/platform/themes/firmware_extract.hpp` (interface header)

**Analog:** itself at Phase 1 — lines 1-54.

**Pattern to preserve** (lines 1-6, 13-22): `#pragma once`, only `<string>`, `namespace thomaz`, struct members are plain `bool`/`std::string`. No libnx types ever enter this header.

**Addition for Phase 2** — add `ExtractAllResult` alongside or replace `ExtractResult`:
```cpp
#pragma once
#include <string>
#include <vector>

namespace thomaz {

struct ExtractResult {           // kept for single-target compat if needed
    bool        ok;
    std::string error;
};

struct ExtractAllResult {        // Phase 2 multi-target result
    bool ok;                                 // false = systemic abort (D-02a)
    std::string systemic_error;
    std::vector<std::string> failed_parts;   // per-part failure messages (D-02)
    std::vector<std::string> written_parts;  // paths written successfully (D-03)
};

ExtractAllResult extract_all_base_layouts(); // Phase 2 entry point
ExtractResult    extract_base_layout(const std::string& target); // Phase 1 spike — keep or remove

} // namespace thomaz
```

---

### `source/platform/themes/nca_extract_switch.cpp` (NCA facade, real)

**Analog:** itself at Phase 1 — lines 1-278.

**Key change: widen `nca_romfs_filter` from exact-name match to `/lyt/` prefix match** (D-01).

**Current filter pattern** (lines 43-48):
```cpp
static bool nca_romfs_filter(void* context, const char* file_name) {
    if (!context || !file_name) return false;
    const auto* ctx = static_cast<const CaptureCtx*>(context);
    const auto& list = *ctx->filter_list;
    return std::find(list.begin(), list.end(), std::string(file_name)) != list.end();
}
```

**Phase 2 replacement — prefix-match for "/lyt/" directory:**
```cpp
static bool nca_romfs_filter(void* context, const char* file_name) {
    if (!context || !file_name) return false;
    const auto* ctx = static_cast<const CaptureCtx*>(context);
    const auto& list = *ctx->filter_list;
    std::string name(file_name);
    for (const auto& entry : list) {
        // If the entry ends with '/' treat it as a directory prefix
        if (!entry.empty() && entry.back() == '/') {
            if (name.find(entry) == 0) return true;
        } else {
            if (name == entry) return true;
        }
    }
    return false;
}
```
Caller passes `filter_list = {"/lyt/"}` to capture all `/lyt/*.szs`. Passing a full filename still works (backward compat with single-target callers).

**Everything else in this file is unchanged:** `nca_on_file_dumped`, `CaptureCtx`, the hactool context setup (lines 110-160), the recovery guard (lines 224-231), the header-key wipe (line 235), the stderr redirect (lines 166-178), and the output validation (lines 263-272).

---

### `source/platform/themes/key_loader_switch.cpp` (privileged service)

**Analog:** itself at Phase 1 — lines 1-265. No changes needed for Phase 2 iteration.

**`resolve_nca_path` already accepts any title-ID string** (lines 213-261). The per-title loop in `firmware_extract_switch.cpp` calls it with `"0100000000001000"`, `"0100000000001007"`, `"0100000000001013"` in turn.

**`open_privileged_session_and_derive_key` and `close_privileged_session` are unchanged** — BIS is mounted once and shared across calls since there is one `g_session` state. The caller must call `close_privileged_session()` exactly once after all titles are processed.

**No changes required to this file.** Document this fact so the planner does not create a no-op plan for it.

---

### `source/platform/themes/cfw_paths.cpp` (path/mapping util)

**Analog:** itself at Phase 1 — lines 1-57.

**The single concrete change: add `common` to `target_map()`** (D-01a). Current `target_map()` at lines 22-32 has 7 entries; `common.szs` is the 8th qlaunch part.

**Current pattern to extend** (lines 24-32):
```cpp
std::optional<TargetMap> target_map(const std::string& target) {
    if (target == "ResidentMenu") return TargetMap{"0100000000001000", "ResidentMenu.szs"};
    if (target == "Entrance")     return TargetMap{"0100000000001000", "Entrance.szs"};
    if (target == "Flaunch")      return TargetMap{"0100000000001000", "Flaunch.szs"};
    if (target == "Set")          return TargetMap{"0100000000001000", "Set.szs"};
    if (target == "Notification") return TargetMap{"0100000000001000", "Notification.szs"};
    if (target == "Psl")          return TargetMap{"0100000000001007", "Psl.szs"};
    if (target == "MyPage")       return TargetMap{"0100000000001013", "MyPage.szs"};
    return std::nullopt;
}
```

**Add before the `return std::nullopt` line:**
```cpp
    if (target == "common")       return TargetMap{"0100000000001000", "common.szs"};
```

**Source of truth for reconciliation: `lib/switchthemes/Common.cpp`** — `ThemeTargetInfo::QlaunchCommon` (line 57-59) confirms `common.szs` belongs to `QlaunchID = 0x0100000000001000`. The `ThemeTargetList6` map (lines 33-42) confirms the other 7 entries match `target_map()` exactly. After adding `common`, `target_map()` and `ThemeTargetInfo` are in full agreement.

**`base_present_for` and `output_szs_path` require no changes** — they derive from `target_map()` automatically.

**`base_layout_dir()` and `base_szs_path()` require no changes.**

---

### `lib/switchthemes/Common.{hpp,cpp}` (vendored mapping tables — read-only reference)

**Role:** These files are the authoritative upstream source for title-ID/szs mappings. Phase 2 reads them for reconciliation; they are NOT modified.

**Key facts extracted for the planner:**

`ThemeTargetInfo` constants to reconcile against `target_map()` (Common.cpp:33-60):
```cpp
// ThemeTargetList6 (fw 6.0+) — maps nxTheme short-names to title+szs
{"home", { QlaunchID,   "Home menu",         "/lyt/ResidentMenu.szs" } },
{"lock", { QlaunchID,   "Lock screen",       "/lyt/Entrance.szs" } },
{"apps", { QlaunchID,   "All apps menu",     "/lyt/Flaunch.szs" } },
{"set",  { QlaunchID,   "Settings applet",   "/lyt/Set.szs" } },
{"news", { QlaunchID,   "News applet",       "/lyt/Notification.szs" } },
{"user", { UserPageID,  "User page",         "/lyt/MyPage.szs" } },
{"psl",  { PslID,       "Player selection",  "/lyt/Psl.szs" } },
// QlaunchCommon — separate static field, NOT in ThemeTargetList6:
ThemeTargetInfo::QlaunchCommon = { QlaunchID, "Home menu common layout", "/lyt/common.szs" }
```

Title-ID constants (Common.hpp:71-73):
```cpp
static constexpr u64 QlaunchID  = 0x0100000000001000;
static constexpr u64 PslID      = 0x0100000000001007;
static constexpr u64 UserPageID = 0x0100000000001013;
```

`GetTargetsForTitleId(tid)` (Common.cpp:89-104) is the upstream analog of `target_map()` — it returns a list of `/lyt/*.szs` paths for a title. For `QlaunchID` it prepends `QlaunchCommon.SzsFile` (`/lyt/common.szs`) then adds the 5 ThemeTargetList6 entries. This is the pattern to use for building the per-title filter lists in the Phase 2 loop.

**Validation primitives** (Yaz0.hpp, Sarc.hpp — already in tree):
```cpp
// D-04 structural validation: Yaz0-decompress then SARC-unpack
#include "SarcLib/Yaz0.hpp"   // Yaz0::IsYaz0(span), Yaz0::Decompress(vec) -> vec
#include "SarcLib/Sarc.hpp"   // SARC::Unpack(span) -> SarcData{files, endianness, HashOnly}

// Upgrade the Phase 1 magic-only check (is_valid_szs) to the D-04 structural check:
bool is_structurally_valid_szs(const std::vector<uint8_t>& buf) {
    if (buf.size() < 4) return false;
    try {
        std::vector<uint8_t> raw = buf;
        if (Yaz0::IsYaz0(raw)) raw = Yaz0::Decompress(raw);   // throws on corrupt Yaz0
        SARC::SarcData sd = SARC::Unpack(raw);                 // throws on invalid SARC
        return !sd.files.empty();
    } catch (...) {
        return false;
    }
}
```
These libs are already compiled into the Switch target via the apply path. No CMake change needed to link them.

---

## Shared Patterns

### switch/fake whole-file `#ifdef` split (unchanged from Phase 1)
**Source:** `source/platform/themes/firmware_extract_switch.cpp:1-6,169` + `firmware_extract_fake.cpp:1-13`
**Apply to:** all `*_switch.cpp` / `*_fake.cpp` pairs — all libnx/hactool headers inside `#ifdef __SWITCH__`; interface header carries no libnx types; fake file carries `#ifndef __SWITCH__` only.

### Applet gate runs first, before any service init (unchanged)
**Source:** `source/platform/themes/firmware_extract_switch.cpp:64-68`
```cpp
if (appletGetAppletType() != AppletType_Application) {
    return {false, "Relaunch thomaz via title takeover (hold R while opening a game) to extract."};
}
```
**Apply to:** the new multi-target entry point in `firmware_extract_switch.cpp`. Still the very first check.

### Single privileged session across all titles (Phase 2 key pattern — D-02a)
**Source:** `source/platform/themes/key_loader_switch.cpp:81-205` — `open_privileged_session_and_derive_key()` + `g_session` state + `close_privileged_session()`
**Apply to:** `firmware_extract_switch.cpp` outer loop. Call `open_privileged_session_and_derive_key()` ONCE before iterating titles; call `close_privileged_session()` ONCE in all exit paths (success AND per-title-failure). BIS mount is expensive — mount once and reuse.

### Per-part best-effort failure collection (D-02)
**Source:** no direct analog — new Phase 2 pattern. Modeled on the spirit of `save_service_switch.cpp:214-218` (never leave a partial artifact; surface a clean error).
**Apply to:** the inner loop in `firmware_extract_switch.cpp`. On per-part failure: push message to `failed_parts`, `continue` to next part. Only return `ok=false` on systemic errors (applet gate, BIS mount, key derivation).

### Canonical output path — flat `/themes/systemData/<szs>` (unchanged, D-03/Pitfall-6)
**Source:** `source/platform/themes/cfw_paths.cpp:14-19` (`base_layout_dir()` = `/themes/systemData`) + `firmware_extract_switch.cpp:33-48` (`ensure_parent_dirs` + `write_file`)
**Apply to:** all per-part write steps. The filename is the SARC basename from the RomFS key (`/lyt/common.szs` → `common.szs`), NOT any exelix subdir layout.

### D-04 structural validation upgrade path
**Source:** `lib/switchthemes/SarcLib/Yaz0.hpp:9-11` + `lib/switchthemes/SarcLib/Sarc.hpp:27`
**Apply to:** the per-part validation step in `firmware_extract_switch.cpp`. Replace the Phase 1 4-byte magic check (`is_valid_szs`) with the full Yaz0-decompress + SARC-unpack check (`is_structurally_valid_szs`) using the already-linked SarcLib. No link-line changes required.

### target_map() is the apply-path contract — additions must not break it
**Source:** `source/platform/themes/cfw_paths.cpp:22-32`; downstream consumer `base_present_for()` at lines 46-55.
**Apply to:** the `common` addition to `target_map()`. Adding a new arm (`if (target == "common") ...`) is additive — existing callers that never ask for "common" are unaffected. Verify by searching for `target_map(` callers before committing.

## No Analog Found

All Phase 2 files have direct Phase 1 analogs (themselves). No greenfield files require research-pattern-only guidance.

| Pattern Gap | Note |
|-------------|------|
| Per-title NCA multi-part iteration | The loop structure is new but built by composing existing primitives (`resolve_nca_path`, `extract_szs_from_nca`, `is_valid_szs`, `write_file`) already proven in Phase 1. |
| Host doctest for `cfw_paths` pure logic | No existing test harness found for `cfw_paths.cpp`. The planner should note that `target_map()` / `base_szs_path()` / `base_present_for()` are pure-function testable on desktop without any libnx dependency — use the desktop build path. |

## Metadata

**Analog search scope:** `source/platform/themes/`, `lib/switchthemes/`, `lib/switchthemes/SarcLib/`
**Files scanned:** firmware_extract_switch.cpp, firmware_extract_fake.cpp, firmware_extract.hpp, nca_extract_switch.cpp, nca_extract_switch.hpp, key_loader_switch.cpp, key_loader_switch.hpp, cfw_paths.cpp, cfw_paths.hpp, lib/switchthemes/Common.hpp, lib/switchthemes/Common.cpp, lib/switchthemes/SarcLib/Yaz0.hpp, lib/switchthemes/SarcLib/Sarc.hpp
**Pattern extraction date:** 2026-06-05
