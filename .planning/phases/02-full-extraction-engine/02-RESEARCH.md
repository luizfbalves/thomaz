# Phase 2: Full Extraction Engine - Research

**Researched:** 2026-06-05
**Domain:** Nintendo Switch homebrew (C++20, Borealis/libnx) — multi-title NCA RomFS extraction
**Confidence:** HIGH (brownfield: all primitives proven in Phase 1; codebase read directly this session)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Extract **every `/lyt/*.szs`** present in each title, not just the 8 canonical
  theme targets. Guarantees the canonical set (ResidentMenu, Entrance, Flaunch, Set,
  Notification, **common** + Psl + MyPage) is always covered and is future-proof. The
  RomFS filter widens from one filename to the whole `/lyt/` directory.
- **D-01a:** `cfw_paths::target_map()` is currently missing **`common`** (7 entries today).
  With D-01 the extraction filter no longer depends on enumerating `target_map()` — but
  `target_map()`/`base_present_for()` (apply path) must still be reconciled so `common` and
  any newly-relevant target resolve correctly.
- **D-02:** **Best-effort + report.** A missing/absent part or title is a warning — extract
  everything that succeeds, collect failures, report which parts/titles failed and why. One
  unavailable `.szs` or optional title does NOT abort the run.
- **D-02a:** **Systemic** failures are a hard abort: if SPL key derivation or the BIS-System
  mount fails, stop the whole run. Distinguish per-part failures (warn, continue) from
  systemic ones (abort).
- **D-03:** **Overwrite each file in place.** Re-running is idempotent and is the correct
  behavior after a firmware update. Do NOT skip existing files (leaves stale szs after an
  update → broken themes); do NOT wipe-then-extract (a mid-run failure would leave no base).
- **D-04:** A `.szs` is accepted only if it **structurally validates**: it must
  Yaz0-decompress and unpack as a valid SARC. Non-empty byte count is NOT sufficient. Reuse
  the already-linked `lib/switchthemes` `Yaz0`/`SARC` for the check. Keep the mapping/path
  logic host-testable (success criterion 4).

### Claude's Discretion

- HOW to structure the loop (mount BIS once and reuse across all three titles vs per-title),
  NCA resolution, and where validation runs in the pipeline.
- Whether "all `/lyt/*.szs`" is a directory-glob prefix filter or an enumerated set — as long
  as D-01 holds.

### Deferred Ideas (OUT OF SCOPE)

- **Firmware-version recording** (`ver.cfg`) and re-extract-after-update UX → Phase 3 (INTEG-03/04).
- **"Extrair layouts do firmware" UI action**, progress reporting, success/failure messaging → Phase 3 (INTEG-01/04/05).
- **qlaunch IPS patch installation** (already wired at apply time) — Phase 3 integration nicety.
- The forwarder → Phase 4 (TAKEOVER-03).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EXTRACT-01 | Extract qlaunch home-menu layouts (ResidentMenu, Entrance, Flaunch, Set, Notification, **common**) from running firmware to SD | Pattern §"Per-title loop" + D-01 `/lyt/` prefix filter captures all qlaunch parts incl. `common`; title `0100000000001000`. `common` added to `target_map()` (D-01a). |
| EXTRACT-02 | Extract the player-select (Psl) layout | Same loop iterates title `0100000000001007`; `Psl.szs` already in `target_map()`. |
| EXTRACT-03 | Extract the MyPage layout | Same loop iterates title `0100000000001013`; `MyPage.szs` already in `target_map()`. |

EXTRACT-04 (keyless / SPL header key, no `prod.keys`) is already satisfied in Phase 1 and is carried forward unchanged by reusing `open_privileged_session_and_derive_key()` and the public KAEK source already in `nca_extract_switch.cpp`.
</phase_requirements>

## Summary

This is a **brownfield generalization phase**, not greenfield. Phase 1 already proved the
entire vertical slice on the codebase: applet gate → BIS mount → `lr` NCA resolve → SPL key
derive → in-memory NCA RomFS extract (vendored hactool fork) → validate → flat write. Every
primitive Phase 2 needs already exists, compiles into the Switch target, and was exercised
host-side or designed for hardware this session. The work is composing those proven primitives
into a multi-title / multi-part driver and tightening two seams: the RomFS filter (single
filename → `/lyt/` prefix) and validation (4-byte magic → full Yaz0+SARC structural check).

The five knowledge gaps the additional-context flagged are all resolved by reading the tree:
(1) **NCA RomFS `/lyt/` filtering** — hactool passes the filter a leading-slash absolute RomFS
path (`/lyt/common.szs`), confirmed in `lib/hactool/source/nca.c:1743` + `filepath.c:69`; a
`name.find("/lyt/") == 0` prefix match is correct and backward-compatible with the single-name
callers. (2) **Multi-title iteration** — `resolve_nca_path(title_id)` already accepts any
title-ID string and `open/close_privileged_session` use one shared `g_session`, so the BIS
mount + key derive happen **once** before the title loop. (3) **Per-part best-effort** — collect
into `failed_parts`/`written_parts` vectors and `continue`; only systemic failures (applet gate,
key derive, BIS mount) return `ok=false`. (4) **Structural validation** — `Yaz0::IsYaz0` /
`Yaz0::Decompress` / `SARC::Unpack` are already linked via the apply path; wrap in try/catch.
(5) **Flat overwrite** — `write_file` already opens `std::ios::trunc`, which IS overwrite-in-place;
filename is the SARC basename of the RomFS key, written flat into `base_layout_dir()`.

**Primary recommendation:** Add a `extract_all_base_layouts()` driver to
`firmware_extract_switch.cpp` that opens the privileged session once, loops the three title-IDs
with a `{"/lyt/"}` prefix filter, structurally validates each captured buffer with SarcLib, and
overwrite-writes the basename flat into `/themes/systemData/`. Add `common` to `target_map()`.
Add a host doctest for the pure `cfw_paths` mapping logic. No new libraries, no CMake link
changes, no external packages.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Applet-vs-Application gate | Privileged service (`firmware_extract_switch.cpp`) | — | Must run before any FS/SPL init; libnx `appletGetAppletType()`. |
| BIS-System mount + SPL key derive | Privileged service (`key_loader_switch.cpp`) | — | One shared `g_session`; opened once, reused across titles (D-02a). |
| Per-title NCA path resolution | Privileged service (`key_loader_switch.cpp::resolve_nca_path`) | — | `lr` resolve; already title-ID-agnostic. |
| NCA RomFS decrypt + `/lyt/` filter | NCA facade (`nca_extract_switch.cpp`) | vendored hactool fork (`lib/hactool`) | In-memory decrypt; filter widens to directory prefix. |
| szs structural validation | Pure logic (`firmware_extract_switch.cpp` helper) | vendored SarcLib (`lib/switchthemes`) | Yaz0+SARC parse; already linked, no libnx. |
| title-ID → szs mapping / path resolution | Pure logic (`cfw_paths.cpp`) | — | Host-testable; no privileged calls (success criterion 4). |
| Flat overwrite write to SD | Pure logic (`firmware_extract_switch.cpp` helpers) | POSIX `mkdir`/`ofstream` | `std::ios::trunc` = overwrite-in-place (D-03). |
| Multi-title/part orchestration + failure collection | Privileged service (`firmware_extract_switch.cpp` driver) | — | New Phase 2 loop composing all the above. |

## Standard Stack

No external packages are installed in this phase. Everything required is already vendored and
linked into the Switch build. This table documents the **in-tree** primitives the planner will
compose.

### Core (already in tree, already linked)
| Component | Location | Purpose | Why Standard |
|-----------|----------|---------|--------------|
| Applet gate | libnx `appletGetAppletType()` | TAKEOVER-01 guard | Phase 1 invariant; first check in the driver. |
| Privileged session | `key_loader_switch.cpp` `open_/close_privileged_session_and_derive_key()` | BIS mount + SPL 0x20-byte header key | EXTRACT-04 keyless path; one `g_session`. |
| NCA path resolve | `key_loader_switch.cpp` `resolve_nca_path(title_id)` | `lr` → `System:/Contents/...nca` | Already accepts any 16-hex title-ID string. |
| NCA RomFS extract | `nca_extract_switch.cpp` `extract_szs_from_nca(path, key, filter)` | in-memory decrypt + filtered capture | Vendored exelix hactool fork @ 2618b0c, isolated-mbedtls build. |
| szs structural validate | `lib/switchthemes/SarcLib` `Yaz0::*` + `SARC::Unpack` | D-04 Yaz0-decompress + SARC-unpack | Already compiled into Switch target via apply path; **no CMake change**. |
| Path/mapping | `cfw_paths.cpp` `target_map / base_layout_dir / base_szs_path / base_present_for` | flat `/themes/systemData/` layout | Shared with the apply path; pure, host-testable. |
| Flat write | `firmware_extract_switch.cpp` `ensure_parent_dirs` + `write_file` | `mkdir -p` + binary `trunc` write | FAT-safe; `trunc` already implements D-03 overwrite. |

### Host test harness (already in tree)
| Component | Location | Purpose | When to Use |
|-----------|----------|---------|-------------|
| doctest | `lib/doctest/doctest.h` | desktop unit tests | success criterion 4 — cover pure mapping/path logic. |
| Existing test models | `tests/test_theme_paths.cpp`, `tests/test_build_id.cpp` | pattern to copy | `#include "doctest.h"` + `#include "platform/themes/cfw_paths.hpp"`, `using namespace thomaz`, `TEST_CASE`/`CHECK`. |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `{"/lyt/"}` prefix filter (D-01) | Enumerated `{"/lyt/ResidentMenu.szs", ...}` list | Enumeration re-introduces the `target_map()` coupling D-01 removes and breaks future-proofing. D-01 explicitly chooses the prefix. Builder may still enumerate per discretion, but prefix is simpler and is the design in PATTERNS.md. |
| One BIS mount reused across titles | Per-title mount/unmount | Per-title re-mount is wasteful and re-runs the expensive SPL derive 3×. PATTERNS.md + D-02a choose mount-once. |
| Full Yaz0+SARC validation (D-04) | 4-byte magic check (Phase 1 `is_valid_szs`) | Magic-only passes a truncated/corrupt szs. D-04 mandates the structural check; cost is one in-memory decompress+parse per part. |

**Installation:** None. No `npm`/`pip`/`cargo`/dkp-pacman step. The hactool fork, isolated
mbedtls, and SarcLib are already wired in `CMakeLists.txt` (lines 55-97, 113-117).

## Package Legitimacy Audit

**Not applicable.** This phase installs zero external packages. All dependencies (vendored
hactool fork, custom mbedtls, exelix SarcLib, doctest, borealis, libnx) are already in-tree and
already linked, established before Phase 2. No registry (npm/PyPI/crates) is touched. slopcheck
/ registry verification has no inputs to check.

## Architecture Patterns

### System Architecture Diagram

```
extract_all_base_layouts()  [driver, firmware_extract_switch.cpp]
        │
        ▼
  (1) Applet gate ── not Application ──► HARD ABORT {ok=false, "relaunch via takeover"}
        │ Application
        ▼
  (2) setsysGetFirmwareVersion  (log once, applies to whole run)
        │
        ▼
  (3) open_privileged_session_and_derive_key()  ── error / key != 0x20 ──► close + HARD ABORT (D-02a)
        │ 0x20-byte header key  (BIS mounted ONCE, g_session)
        ▼
  ┌─────────────────────────────────────────────────────────────────────┐
  │ FOR title_id IN { …1000 (qlaunch), …1007 (Psl), …1013 (MyPage) }     │
  │     resolve_nca_path(title_id) ── empty ──► failed_parts += ; continue│  (D-02 per-part)
  │         │ nca_path                                                    │
  │         ▼                                                             │
  │     extract_szs_from_nca(nca_path, header_key, {"/lyt/"})             │
  │         │   ── error ──► failed_parts += ; continue                   │
  │         ▼  files: { "/lyt/<name>.szs" -> bytes, ... }                 │
  │     FOR each (romfs_key, buf) IN files:                               │
  │         is_structurally_valid_szs(buf)  ── false ──► failed_parts +=  │  (D-04)
  │             │ valid                                                   │
  │             ▼                                                         │
  │         out = base_layout_dir() + "/" + basename(romfs_key)          │
  │         ensure_parent_dirs(out); write_file(out, buf)  [trunc=O/W]    │  (D-03)
  │             │ ok ──► written_parts += out                            │
  │             └ fail ──► failed_parts += out                           │
  └─────────────────────────────────────────────────────────────────────┘
        │ all titles processed
        ▼
  (N) close_privileged_session()   [exactly once, all exit paths]
        ▼
  return { ok=true, systemic_error="", failed_parts, written_parts }
```

File-to-implementation mapping is in the Component Responsibilities (Standard Stack) tables, not
the diagram.

### Recommended Project Structure (unchanged — all files exist)
```
source/platform/themes/
├── firmware_extract.hpp          # add ExtractAllResult + extract_all_base_layouts()
├── firmware_extract_switch.cpp   # add the driver; widen validation to structural
├── firmware_extract_fake.cpp     # add matching no-op for the new entry point
├── nca_extract_switch.cpp        # widen nca_romfs_filter to /lyt/ prefix match
├── key_loader_switch.cpp         # NO CHANGE (resolve_nca_path already title-agnostic)
└── cfw_paths.cpp                 # add `common` to target_map()
lib/switchthemes/SarcLib/         # Yaz0/SARC — reused read-only for D-04 validation
tests/
└── test_cfw_paths.cpp            # NEW host doctest for pure mapping/path logic
```

### Pattern 1: Single privileged session across all titles (D-02a)
**What:** Open BIS + derive SPL key once before the title loop; close once after.
**When to use:** The multi-title driver. BIS mount is expensive and `g_session` is single-state.
```cpp
// Source: key_loader_switch.cpp open_/close_privileged_session_and_derive_key (Phase 1)
KeyDerivationOutput kdo = open_privileged_session_and_derive_key();
if (!kdo.error.empty() || kdo.header_key.size() != 0x20) {
    close_privileged_session();
    return {false, "Key derivation failed: " + kdo.error, {}, {}};  // D-02a hard abort
}
for (const std::string& title_id : {std::string("0100000000001000"),
                                    std::string("0100000000001007"),
                                    std::string("0100000000001013")}) {
    // ... per-title work, reusing kdo.header_key ...
}
close_privileged_session();   // exactly once, all exit paths
```

### Pattern 2: `/lyt/` directory-prefix RomFS filter (D-01)
**What:** Widen the exact-name filter to capture every `.szs` under `/lyt/`.
**When to use:** `nca_romfs_filter` in `nca_extract_switch.cpp`. The caller passes `{"/lyt/"}`.
**Verified detail:** hactool passes the filter the **leading-slash absolute RomFS path**, e.g.
`/lyt/common.szs`. Confirmed: `lib/hactool/source/nca.c:1743` calls
`romfs_filter(extra_context, cur_path->char_path)`, and `filepath_append` (`filepath.c:69`)
`strcat`s `OS_PATH_SEPARATOR` ("/") before each segment, seeded from an empty root — so the
first segment yields a leading `/`. This is exactly the `"/lyt/" + szs` form Phase 1 already
matched, so a `find(...) == 0` prefix is correct AND backward-compatible with single-name callers.
```cpp
// Source: PATTERNS.md replacement for nca_extract_switch.cpp:43-48
static bool nca_romfs_filter(void* context, const char* file_name) {
    if (!context || !file_name) return false;
    const auto* ctx = static_cast<const CaptureCtx*>(context);
    const auto& list = *ctx->filter_list;
    std::string name(file_name);
    for (const auto& entry : list) {
        if (!entry.empty() && entry.back() == '/') {          // directory prefix
            if (name.rfind(entry, 0) == 0) return true;       // starts_with(entry)
        } else {
            if (name == entry) return true;                   // exact (single-target compat)
        }
    }
    return false;
}
```

### Pattern 3: Structural szs validation (D-04)
**What:** Replace the 4-byte magic check with a full Yaz0-decompress + SARC-unpack.
**When to use:** Per-part, after capture, before write.
**Verified signatures (read this session):**
- `bool Yaz0::IsYaz0(std::span<const u8>)` — `lib/switchthemes/SarcLib/Yaz0.hpp:9`
- `std::vector<u8> Yaz0::Decompress(const std::vector<u8>&)` — throws on corrupt Yaz0
- `SARC::SarcData SARC::Unpack(std::span<const u8>)` — `Sarc.hpp:27`; `SarcData.files` is the unpacked map; throws on invalid SARC
```cpp
// Source: PATTERNS.md "Validation Primitives"; signatures confirmed in Yaz0.hpp/Sarc.hpp
#include "SarcLib/Yaz0.hpp"
#include "SarcLib/Sarc.hpp"
bool is_structurally_valid_szs(const std::vector<std::uint8_t>& buf) {
    if (buf.size() < 4) return false;
    try {
        std::vector<std::uint8_t> raw = buf;
        if (Yaz0::IsYaz0(raw)) raw = Yaz0::Decompress(raw);  // throws on corrupt Yaz0
        SARC::SarcData sd = SARC::Unpack(raw);               // throws on invalid SARC
        return !sd.files.empty();
    } catch (...) {
        return false;
    }
}
```
Note: `u8` is `std::uint8_t` (via `lib/switchthemes/MyTypes.h`). The buffers from
`NcaExtractResult.files` are `std::vector<std::uint8_t>`; pass directly (an implicit
`std::span<const u8>` is constructed for `IsYaz0`/`Unpack`).

### Pattern 4: Flat overwrite-in-place write (D-03)
**What:** Write the SARC basename flat into `base_layout_dir()`, truncating any existing file.
**When to use:** Per validated part.
```cpp
// Source: firmware_extract_switch.cpp:33-48 (ensure_parent_dirs + write_file)
// write_file already opens std::ios::binary | std::ios::trunc  ==  overwrite-in-place (D-03).
std::string romfs_key = "/lyt/common.szs";                 // from NcaExtractResult key
std::string base = romfs_key.substr(romfs_key.rfind('/') + 1);  // "common.szs"
std::string out  = base_layout_dir() + "/" + base;         // "/themes/systemData/common.szs"
ensure_parent_dirs(out);
if (write_file(out, buf)) written_parts.push_back(out);
else                      failed_parts.push_back(out + ": write failed");
```
**Critical:** derive the output filename from the RomFS basename, NOT from any exelix
`extracted/{qlaunch,…}/` subdir layout. Flat `/themes/systemData/<szs>` is success criterion 3.

### Pattern 5: Host doctest for pure mapping logic (success criterion 4)
**What:** Desktop-only test of `target_map`/`base_szs_path`/`base_present_for` — no libnx.
```cpp
// Source: model on tests/test_theme_paths.cpp
#include "doctest.h"
#include "platform/themes/cfw_paths.hpp"
using namespace thomaz;
TEST_CASE("target_map resolves all eight qlaunch parts incl. common") {
    CHECK(target_map("common")->title_id == "0100000000001000");
    CHECK(target_map("common")->szs      == "common.szs");
    CHECK(target_map("Psl")->title_id    == "0100000000001007");
    CHECK(target_map("MyPage")->title_id == "0100000000001013");
    CHECK_FALSE(target_map("Nonexistent").has_value());
}
TEST_CASE("base_szs_path is flat under base_layout_dir") {
    CHECK(base_szs_path("common") == base_layout_dir() + "/common.szs");
}
```
`cfw_paths.cpp` compiles on desktop (its `#ifdef __SWITCH__` only swaps the dir string), so it
links into the doctest binary with zero libnx/hactool dependency.

### Anti-Patterns to Avoid
- **Re-mounting BIS per title:** wasteful, re-runs SPL derive 3×, contradicts D-02a. Mount once.
- **Aborting the whole run on a missing part:** violates D-02. Only systemic failures abort.
- **`is_valid_szs` magic-only check for writes:** D-04 requires structural validation. The
  Phase 1 magic helper is insufficient (passes truncated buffers).
- **Nesting output under `extracted/qlaunch/`:** breaks success criterion 3 and the apply path.
- **Skipping existing files OR wipe-then-extract:** both rejected by D-03. Use `trunc` per file.
- **Adding `common` only to the extraction filter but not `target_map()`:** the apply path
  (`base_present_for`) still can't resolve `common` → reconciliation incomplete (D-01a).
- **Calling `nca_init(&nca_ctx)`** in the NCA facade: in this fork it's a `memset` that nulls
  the wired `file`/`tool_ctx` → NULL-FILE data abort. Already documented in
  `nca_extract_switch.cpp:219-223`; do NOT reintroduce.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Yaz0 decompression | a custom RLE decoder | `Yaz0::Decompress` (SarcLib) | Already linked, battle-tested by the apply path. |
| SARC parsing | a custom archive walker | `SARC::Unpack` (SarcLib) | Handles endianness, hash-only entries, alignment. |
| NCA decrypt / RomFS walk | anything | `extract_szs_from_nca` (vendored hactool fork) | XTS/CTR crypto, section parsing, recovery guard, isolated mbedtls — already solved in Phase 1. |
| SPL key derivation | re-deriving keys | `open_privileged_session_and_derive_key` | Public KAEK source + SPL chain already wired (EXTRACT-04). |
| `mkdir -p` / binary write | new FS helpers | `ensure_parent_dirs` + `write_file` (in `firmware_extract_switch.cpp`) | FAT-safe, `trunc`=overwrite, proven Phase 1. |
| title-ID/szs table | a new map | extend `target_map()` (one `common` arm) | Single source of truth shared with apply. |

**Key insight:** Phase 2 adds essentially **zero new algorithms**. Every hard problem (crypto,
compression, archive format, key derivation) is already solved and linked. The phase is a control-
flow generalization plus two one-line seam widenings (filter prefix, structural validate) plus a
one-arm table addition. Treat any task that proposes new crypto/parsing code as a red flag.

## Runtime State Inventory

This is a generalization phase, not a rename/migration. No string is being renamed and no stored
runtime state is being re-keyed. Categories explicitly checked:

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None — output is `.szs` files on SD only; no DB/datastore keyed on any Phase 2 string. | None. |
| Live service config | None — extraction reads firmware NCAs via `lr`; writes nothing to any external service. | None. |
| OS-registered state | None — no Task Scheduler/pm2/systemd registrations involved. | None. |
| Secrets/env vars | None — SPL header key is derived on-device, lives in-memory for the call, wiped after use (`nca_extract_switch.cpp:235`). No env var or SOPS key. | None. |
| Build artifacts | None new. hactool/mbedtls/SarcLib already built and linked; Phase 2 adds no link-line change. The host doctest binary is a build output, not stale state. | None. |

**One reconciliation item (not runtime state, but a shared-contract change):** adding `common`
to `target_map()` changes a table consumed by the apply path (`base_present_for`). It is purely
additive (a new `if` arm); existing callers that never request `"common"` are unaffected. Verify
by grepping `target_map(` callers before committing (PATTERNS.md "target_map() is the apply-path
contract"). Confirmed callers in tree: `base_szs_path`, `output_szs_path`, `base_present_for`
(all in `cfw_paths.cpp`) and `firmware_extract_switch.cpp` — all derive automatically.

## Common Pitfalls

### Pitfall 1: Filter passed the wrong path form
**What goes wrong:** Passing `{"lyt/"}` (no leading slash) or `{"common.szs"}` matches nothing,
producing an empty `captured` map → `extract_szs_from_nca` returns "filter matched nothing".
**Why it happens:** hactool builds the RomFS path with a leading separator from an empty root.
**How to avoid:** Pass `{"/lyt/"}` (leading slash, trailing slash for prefix semantics).
Confirmed against `nca.c:1743` + `filepath.c:69` this session.
**Warning signs:** "extraction returned no files — filter matched nothing in RomFS" in hactool.log.

### Pitfall 2: Closing the session inside the title loop
**What goes wrong:** `close_privileged_session()` called per title tears down BIS; subsequent
titles fail to resolve their NCA.
**Why it happens:** Copy-pasting the Phase 1 single-target teardown into the loop body.
**How to avoid:** Open before the loop, close exactly once after (and on hard-abort paths). The
key (`kdo.header_key`) is reused for every `extract_szs_from_nca` call.
**Warning signs:** First title (qlaunch) succeeds, Psl/MyPage all report NCA-resolve failures.

### Pitfall 3: Treating a missing optional title as a fatal error
**What goes wrong:** Psl/MyPage absent on some firmware → whole run aborts, qlaunch layouts lost.
**Why it happens:** Returning `{false, ...}` on `resolve_nca_path` empty instead of collecting.
**How to avoid:** Per-part failures push to `failed_parts` and `continue` (D-02). Only the applet
gate, key derivation, and BIS mount return `ok=false` (D-02a).
**Warning signs:** `ok=false` with non-empty `written_parts` would be contradictory — that should
never happen; if it does, a per-part path is wrongly treated as systemic.

### Pitfall 4: Magic-only validation slips a corrupt szs to the apply path
**What goes wrong:** A truncated/mis-decrypted buffer that still starts with `Yaz0`/`SARC` magic
passes the Phase 1 check and silently lands in `/themes/systemData/`, breaking a theme later.
**Why it happens:** Reusing `is_valid_szs` (4-byte magic) instead of D-04 structural check.
**How to avoid:** Use `is_structurally_valid_szs` (Yaz0-decompress + SARC-unpack, try/catch).
**Warning signs:** A part validates and writes but the theme engine rejects it on apply.

### Pitfall 5: Desktop build breaks because SarcLib include leaks into a neutral header
**What goes wrong:** `firmware_extract.hpp` (neutral, no libnx) must not include SarcLib or
hactool. Adding `#include "SarcLib/Yaz0.hpp"` to the header would pull `MyTypes.h`/`u8` into the
desktop TU and risk the D-08 invariant.
**Why it happens:** Placing the validation helper or its includes in the interface header.
**How to avoid:** Keep `is_structurally_valid_szs` and its SarcLib includes **inside** the
`#ifdef __SWITCH__` body of `firmware_extract_switch.cpp` (SarcLib is on the Switch include path
via `CMakeLists.txt:109`). The fake TU and the neutral header stay SarcLib-free. The new
`ExtractAllResult` struct uses only `bool`/`std::string`/`std::vector<std::string>`.
**Warning signs:** Desktop/fake compile errors referencing `u8` or `SARC`.

### Pitfall 6: Output filename collision across titles
**What goes wrong:** Two titles each containing a `/lyt/<same-name>.szs` would overwrite one
another in the flat layout.
**Why it happens:** Flat layout has no per-title namespace.
**How to avoid:** Not a real risk for the canonical set — qlaunch/Psl/MyPage `.szs` names are
disjoint (`common`, `ResidentMenu`, …, `Psl.szs`, `MyPage.szs`). With D-01 capturing all
`/lyt/*.szs`, the on-hardware qlaunch dump (~16 files, CONTEXT specifics) names are still unique
within and across titles. If a future firmware introduced a clash, last-writer-wins (D-03
overwrite) is acceptable but worth a log line. Low risk; document, do not over-engineer.

## Code Examples

### ExtractAllResult + driver shape (firmware_extract.hpp)
```cpp
// Source: PATTERNS.md addition; member types are libnx-free (D-08)
struct ExtractAllResult {
    bool ok;                                  // false ONLY on systemic abort (D-02a)
    std::string systemic_error;
    std::vector<std::string> failed_parts;    // per-part warnings (D-02)
    std::vector<std::string> written_parts;   // canonical paths written (D-03)
};
ExtractAllResult extract_all_base_layouts();  // Phase 2 entry point
```

### Desktop no-op (firmware_extract_fake.cpp)
```cpp
// Source: PATTERNS.md — zero libnx symbols, matches save_service_fake.cpp
ExtractAllResult extract_all_base_layouts() {
    return {false, "Firmware extraction is only available on Switch.", {}, {}};
}
```

### Per-part write (basename extraction)
```cpp
// Source: this session — basename of the RomFS key drives the flat filename
for (const auto& [romfs_key, buf] : nca_res.files) {
    if (!is_structurally_valid_szs(buf)) {           // D-04
        failed_parts.push_back(romfs_key + ": invalid szs"); continue;
    }
    std::string base = romfs_key.substr(romfs_key.rfind('/') + 1);
    std::string out  = base_layout_dir() + "/" + base;   // flat (D-03)
    ensure_parent_dirs(out);
    if (write_file(out, buf)) written_parts.push_back(out);
    else                      failed_parts.push_back(out + ": write failed");
}
```

## State of the Art

| Old Approach (Phase 1) | Current Approach (Phase 2) | When Changed | Impact |
|------------------------|----------------------------|--------------|--------|
| `extract_base_layout(target)` single (title, szs) | `extract_all_base_layouts()` multi-title driver | This phase | All eight qlaunch parts + Psl + MyPage in one run. |
| Filter = `{"/lyt/" + szs}` exact name | Filter = `{"/lyt/"}` directory prefix | This phase (D-01) | Future-proof; captures `common` and any new part without code change. |
| `is_valid_szs` (4-byte magic) | `is_structurally_valid_szs` (Yaz0+SARC) | This phase (D-04) | Catches subtly-corrupt extractions before apply. |
| `target_map()` 7 entries | 8 entries (`+ common`) | This phase (D-01a) | Apply path can resolve `common`. |
| Fail-fast on any error | Best-effort + systemic-vs-per-part split | This phase (D-02/D-02a) | One missing optional title no longer wipes the whole run. |

**Deprecated/outdated:** Nothing removed. `extract_base_layout` (single-target spike) may be kept
for backward compat or removed — PATTERNS.md marks it "keep or remove" (builder's discretion; no
caller outside the spike's own validation depends on it).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The on-hardware `/lyt/` set per title is a clean superset of the canonical 8 and every captured buffer parses as valid SARC. | D-01 / Specifics | LOW — CONTEXT specifics report a real fw 22.1.0 dump (~16 szs, all parse). If a part fails to parse, D-02/D-04 already handle it gracefully (warn + skip). |
| A2 | `extract_base_layout` single-target entry point has no external callers and can be kept or removed freely. | State of the Art | LOW — grep `extract_base_layout(` before deciding. Phase 3 UI is not yet built (deferred), so the only caller risk is internal. |
| A3 | Adding `common` to `target_map()` does not break any apply-path caller. | Runtime State Inventory | LOW — purely additive `if` arm; confirmed callers all derive from the table. Verify with `grep -rn 'target_map(' source/`. |

**No `[ASSUMED]` claim here is load-bearing for security or compliance.** All three are
low-risk and self-verifying with a grep or already-budgeted graceful degradation.

## Open Questions

1. **Keep or remove the single-target `extract_base_layout`?**
   - What we know: PATTERNS.md leaves it to the builder; it shares the inner-loop body.
   - What's unclear: whether Phase 3 UI will want a single-target re-extract.
   - Recommendation: Keep it (thin wrapper or leave as-is); cost is negligible and Phase 3
     (INTEG-03 re-extract) may reuse it. Removing it is a clean-up, not a requirement.

2. **Does the driver log per-part failures to `/switch/thomaz/hactool.log` or only return them?**
   - What we know: the NCA facade already redirects hactool stderr to that log.
   - What's unclear: whether the driver should also append a run summary.
   - Recommendation: Return them in `ExtractAllResult` (Phase 3 UI consumes that). A `printf`
     summary is fine for now but UI messaging is explicitly Phase 3 (INTEG-05) — do not build
     user-facing messaging here.

## Environment Availability

> The Phase 2 deliverable is hardware-verified (Switch). The host side only needs a desktop
> C++20 toolchain to build the doctest. The privileged path is Switch-only by design.

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| devkitPro / devkitA64 | Switch build (CI) | per project (CI image `devkitpro/devkita64`) | — | None — Switch build is CI/hardware only. |
| Desktop C++20 compiler | doctest (success criterion 4) | per project | C++20 (CMake sets `CXX_STANDARD 20`) | None needed — pure-logic test, no libnx. |
| borealis submodule | desktop cmake configure | **NOT checked out in this research env** | — | Switch CI build is the real verification; desktop configure fails with `cmake_dependent_option` until submodule present (documented in all Phase 1 summaries). |
| Nintendo Switch hardware (fw, takeover) | success criteria 1-3 | hardware-only | fw 22.1.0 dump referenced | None — extraction is hardware-verified by design. |

**Missing dependencies with no fallback:** Real on-hardware verification of criteria 1-3 requires
a Switch in Application mode (title takeover) — same hardware gate as Phase 1. The planner must
include a `checkpoint:human-verify` task for the hardware run; this is not automatable.

**Missing dependencies with fallback:** Desktop cmake configure may fail locally if the borealis
submodule isn't checked out — this does NOT block Phase 2 code work; the Switch CI build and the
doctest (which compiles `cfw_paths.cpp` + `lib/doctest` directly) are the real gates.

## Validation Architecture

> `nyquist_validation` key absent from `.planning/config.json` init context → treated as enabled.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | doctest (`lib/doctest/doctest.h`) |
| Config file | none — tests are standalone `.cpp` in `tests/`, each `#include "doctest.h"` |
| Quick run command | build + run the single `test_cfw_paths` binary (desktop toolchain) |
| Full suite command | build + run all `tests/test_*.cpp` desktop doctests |

Note: the root `CMakeLists.txt` does **not** wire `tests/` into the app build (it globs only
`source/*.cpp`). Existing tests (`tests/test_theme_paths.cpp`, etc.) are compiled by a desktop
host toolchain against `-Isource -Ilib/doctest`. The planner should confirm/record the exact
host-test invocation the project already uses (a Wave 0 gap — see below) rather than assume a
CMake test target exists.

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| EXTRACT-01 | qlaunch parts incl. `common` resolve to title `…1000` + correct szs | unit (host) | run `test_cfw_paths` (`target_map("common")` etc.) | ❌ Wave 0 |
| EXTRACT-02 | `Psl` resolves to title `…1007` | unit (host) | run `test_cfw_paths` | ❌ Wave 0 |
| EXTRACT-03 | `MyPage` resolves to title `…1013` | unit (host) | run `test_cfw_paths` | ❌ Wave 0 |
| EXTRACT-01/02/03 | flat `base_szs_path` = `base_layout_dir()/<szs>` | unit (host) | run `test_cfw_paths` | ❌ Wave 0 |
| EXTRACT-01/02/03 | full extraction produces all parts on SD | manual (hardware) | `checkpoint:human-verify` Switch run (criteria 1-3) | n/a — hardware only |
| D-04 | structural validation rejects non-SARC buffer | unit (host, optional) | a host test feeding garbage to a host-visible validator | ❌ Wave 0 (optional — validator lives in `_switch.cpp`; see note) |

**Note on D-04 host testability:** `is_structurally_valid_szs` currently lives inside the
`#ifdef __SWITCH__` body and depends on SarcLib (which compiles on desktop). If the planner wants
a host doctest for it (success criterion 4 says pure parsing logic should be covered "where it
does not touch privileged services" — validation does NOT touch privileged services), extract it
into a small neutral helper TU (e.g. `szs_validate.cpp`, no libnx) so both the Switch driver and
the desktop doctest link it. This is the cleanest way to satisfy criterion 4 for validation;
otherwise validation is hardware-verified only. Recommend extracting it.

### Sampling Rate
- **Per task commit:** run `test_cfw_paths` (the pure-logic doctest).
- **Per wave merge:** run the full `tests/test_*.cpp` host suite.
- **Phase gate:** host suite green + the hardware `checkpoint:human-verify` (criteria 1-3) approved.

### Wave 0 Gaps
- [ ] `tests/test_cfw_paths.cpp` — covers EXTRACT-01/02/03 mapping + flat path (and the `common`
  addition). Model on `tests/test_theme_paths.cpp`.
- [ ] **Record the existing host-test build/run command** — the project already runs doctests but
  the invocation isn't in CMake; capture it (script/Makefile in `tests/Makefile` exists — confirm
  it builds individual `test_*.cpp`) so the sampling commands above are concrete.
- [ ] (Recommended) Extract `is_structurally_valid_szs` into a neutral `szs_validate.{hpp,cpp}`
  so D-04 gets a host doctest (`tests/test_szs_validate.cpp` feeding a known-good and a garbage
  buffer). Optional but directly satisfies criterion 4 for validation.

## Security Domain

> `security_enforcement` not disabled in config → included.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | No user auth; privilege comes from title-takeover Application mode (OS-level). |
| V3 Session Management | no | No sessions in the web sense; the privileged `g_session` is an FS/SPL handle, torn down deterministically. |
| V4 Access Control | yes | Applet gate (`appletGetAppletType() == AppletType_Application`) is the access-control boundary for BIS/SPL — first check, before any privileged init (TAKEOVER-01). |
| V5 Input Validation | yes | `extract_szs_from_nca` validates `nca_path` non-empty, `header_key == 0x20`, `filter_list` non-empty; D-04 structurally validates every output buffer before it touches the apply path. |
| V6 Cryptography | yes | NCA header/section decrypt + SPL key derivation are delegated entirely to the vendored hactool fork + isolated mbedtls — **never hand-rolled**. Header key wiped after use (`nca_extract_switch.cpp:235`). |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Privileged action in applet mode (no takeover) | Elevation of Privilege / DoS | Applet gate first; clear relaunch message, no service init attempted (T-01-14). |
| Corrupt/mis-decrypted szs reaches apply path | Tampering / Integrity | D-04 Yaz0+SARC structural validation before write; never overwrite a good file with garbage (T-01-15). |
| Header key leakage via logs/return | Information Disclosure | Key lives in keyset for the call only, wiped immediately; only all-zero flag + non-invertible fold logged, never the value (T-01-04/10). |
| hactool `exit()` on bad key crashes the app | DoS | `setjmp`/`longjmp` recovery guard (`g_hactool_recover_jmp`) turns `exit()` into a clean error (T-01-13). |
| Writing outside the SD theme dir | Elevation of Privilege | Output path is always `base_layout_dir()/<basename>`; no BIS write; `ensure_parent_dirs` + `write_file` only (T-01-16). |
| Partial run leaves user with no base layouts | Availability | D-03 overwrite-in-place (not wipe-then-extract); best-effort writes each validated part independently. |

**New threat surface in Phase 2:** none beyond Phase 1's register. The driver introduces no
network endpoints, no new file-write targets (same `base_layout_dir()`), no auth paths, no schema
changes. It iterates the same proven privileged primitives.

## Sources

### Primary (HIGH confidence — read directly this session)
- `source/platform/themes/firmware_extract_switch.cpp` — Phase 1 single-target pipeline (the inner-loop body).
- `source/platform/themes/nca_extract_switch.cpp` / `.hpp` — NCA facade, current exact-name filter, hactool wiring, recovery guard, key wipe.
- `source/platform/themes/cfw_paths.cpp` / `.hpp` — `target_map()` (7 entries, missing `common`), `base_layout_dir`, `base_szs_path`, `base_present_for`.
- `lib/switchthemes/SarcLib/Yaz0.hpp`, `lib/switchthemes/SarcLib/Sarc.hpp` — confirmed `IsYaz0`/`Decompress`/`Unpack` signatures for D-04.
- `lib/hactool/source/nca.c:1742-1808`, `lib/hactool/source/filepath.c:69-100` — confirmed the romfs_filter receives a leading-slash absolute RomFS path (`/lyt/<name>.szs`).
- `CMakeLists.txt:55-117` — confirms hactool/mbedtls/SarcLib already linked; no new link step.
- `tests/test_theme_paths.cpp`, `tests/test_build_id.cpp`, `lib/doctest/doctest.h` — host doctest pattern.
- `.planning/phases/02-full-extraction-engine/02-CONTEXT.md`, `02-PATTERNS.md` — locked decisions + analog map.
- `.planning/phases/01-privileged-extraction-spike/01-04-SUMMARY.md` — Phase 1 entry-point provenance.
- `.planning/REQUIREMENTS.md`, `.planning/STATE.md` — requirement IDs + project decisions.

### Secondary (MEDIUM confidence)
- CONTEXT.md "Specifics": real fw 22.1.0 on-hardware qlaunch `/lyt/` dump (~16 szs) — single observation, treated as representative (A1).

### Tertiary (LOW confidence)
- None. No WebSearch was needed; this is a closed brownfield codebase with all primitives in tree.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — every component read in-tree and already linked; zero external installs.
- Architecture: HIGH — driver composes proven Phase 1 primitives; loop shape pre-specified in PATTERNS.md and validated against the actual source.
- Pitfalls: HIGH — filter path form and session lifecycle verified against hactool source and `g_session` usage; not training-data assumptions.
- Validation/test wiring: MEDIUM — doctest harness exists, but the exact host build/run command isn't in CMake (Wave 0 gap to capture).

**Research date:** 2026-06-05
**Valid until:** ~2026-07-05 (stable — vendored deps pinned; only a future firmware changing the `/lyt/` set or title-IDs would invalidate, and D-01/D-02 already degrade gracefully).
