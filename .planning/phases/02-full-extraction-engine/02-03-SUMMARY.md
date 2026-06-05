---
phase: 02-full-extraction-engine
plan: "03"
subsystem: platform/themes extraction engine
tags: [firmware-extract, nca-extract, switch, romfs-filter, best-effort]
dependency_graph:
  requires: ["02-01", "02-02"]
  provides: ["extract_all_base_layouts", "nca_romfs_filter-prefix"]
  affects: ["firmware_extract_switch.cpp", "nca_extract_switch.cpp"]
tech_stack:
  added: []
  patterns:
    - "D-01 directory-prefix RomFS filter (entry.back()=='/' + rfind(entry,0)==0)"
    - "D-02 best-effort per-part failure collection (failed_parts + continue)"
    - "D-02a systemic-abort gate before any service init (applet gate + key-derive failure)"
    - "D-03 overwrite-in-place flat write to base_layout_dir()"
    - "D-04 is_structurally_valid_szs (Yaz0+SARC) as write gate (Plan 02 primitive)"
    - "Single privileged session opened once outside the title loop, closed once on all exit paths"
key_files:
  modified:
    - source/platform/themes/nca_extract_switch.cpp
    - source/platform/themes/firmware_extract_switch.cpp
decisions:
  - "nca_romfs_filter prefix check uses rfind(entry,0)==0 (starts_with semantics) not find() — avoids matching /lyt/ in a hypothetical /not/lyt/path"
  - "filter_list const variable {'/lyt/'} declared outside the per-title loop — shared across all three titles, no per-iteration allocation"
  - "kTitleIds array declared as static const char* const[] — three titles in qlaunch/Psl/MyPage order per RESEARCH.md diagram"
  - "close_privileged_session() NOT inside the per-title loop (Pitfall 2) — appears only on key-derive-abort paths and once after the loop"
metrics:
  duration: "2 minutes"
  completed: "2026-06-05"
  tasks_completed: 2
  tasks_total: 2
  files_changed: 2
---

# Phase 02 Plan 03: Multi-Title Extraction Driver + /lyt/ Filter Widening Summary

**One-liner:** `/lyt/` directory-prefix RomFS filter and `extract_all_base_layouts()` single-session multi-title driver using Phase 1 primitives with D-04 structural validation and D-03 flat overwrite.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Widen nca_romfs_filter to /lyt/ directory-prefix match (D-01) | 1785033 | source/platform/themes/nca_extract_switch.cpp |
| 2 | Implement extract_all_base_layouts() multi-title best-effort driver | d5bfc11 | source/platform/themes/firmware_extract_switch.cpp |

## What Was Built

### Task 1 — nca_romfs_filter prefix widening

Replaced the `std::find` exact-name match in `nca_romfs_filter` with a prefix-aware loop:

- An entry whose `back() == '/'` is treated as a directory prefix; `name.rfind(entry, 0) == 0` (starts_with semantics) returns true for any RomFS path that begins with the entry.
- Exact-name match (`name == entry`) is the fallback for non-slash entries, preserving the single-target Phase 1 caller that passes `/lyt/ResidentMenu.szs` directly.
- Null guards (`!context || !file_name`) are retained.
- No other function in `nca_extract_switch.cpp` was modified; the key wipe, recovery guard, and dump callback are byte-identical. The diff contains only one hunk, inside `nca_romfs_filter`.

### Task 2 — extract_all_base_layouts() driver

Added `ExtractAllResult extract_all_base_layouts()` to `firmware_extract_switch.cpp` inside the `#ifdef __SWITCH__` block alongside `extract_base_layout`. Control flow exactly per the RESEARCH.md diagram:

1. **Applet gate first** (T-02-07/TAKEOVER-01): `appletGetAppletType() != AppletType_Application` returns `ok=false` with the relaunch message before any `setsys`/SPL/BIS init.
2. **Firmware version captured once** via `setsys{Initialize,GetFirmwareVersion,Exit}` and printed — applies to the whole multi-title run.
3. **Single privileged session** opened via `open_privileged_session_and_derive_key()` before the title loop. Key-derivation error (non-empty `kdo.error`) or unexpected key length (not `0x20`) causes a systemic abort: `close_privileged_session()` + return `ok=false` (D-02a).
4. **Per-title loop** over `0100000000001000`, `0100000000001007`, `0100000000001013`:
   - `resolve_nca_path(title_id)` empty → push `title_id + ": NCA resolve failed"` to `failed_parts`, `continue` (D-02).
   - `extract_szs_from_nca(nca_path, kdo.header_key, {"/lyt/"})` error → push to `failed_parts`, `continue` (D-02).
   - Per-file in `res.files`: `is_structurally_valid_szs(buf)` false → push `romfs_key + ": invalid szs"` to `failed_parts`, `continue` (D-04/T-02-08). On pass: compute `base = romfs_key.substr(romfs_key.rfind('/') + 1)`, `out = base_layout_dir() + "/" + base`, `ensure_parent_dirs(out)`, `write_file(out, buf)` — push to `written_parts` on success or `out + ": write failed"` to `failed_parts` (D-03/T-02-09).
5. **Session closed exactly once** after the loop (T-02-10). `close_privileged_session()` is never inside the per-title loop (Pitfall 2).
6. Returns `{true, {}, failed_parts, written_parts}` — `ok=true` with per-part warnings is the normal best-effort outcome.

`szs_validate.hpp` was added as an include (neutral header, outside the `#ifdef __SWITCH__` guard — consistent with `firmware_extract.hpp`, `nca_extract_switch.hpp`, and `cfw_paths.hpp` which are also included before the guard).

`extract_base_layout` and its live caller (`theme_detail_activity.cpp`) are unchanged — the new driver is additive.

## Threat Mitigations Implemented

| Threat ID | Mitigation Applied |
|-----------|-------------------|
| T-02-07 | Applet gate is the first statement in `extract_all_base_layouts()`; precedes all service init |
| T-02-08 | `is_structurally_valid_szs` (D-04 Yaz0+SARC) used as write gate; invalid buffers go to `failed_parts` |
| T-02-09 | Output always `base_layout_dir() + "/" + basename(romfs_key)`; only `rfind('/')+1` used; no `..` survives |
| T-02-10 | `close_privileged_session()` on key-derive abort paths and exactly once after the loop; never inside the loop |
| T-02-11 | D-03 overwrite-in-place; a missing optional title does not abort; already-written parts remain intact |
| T-02-SC | No package installs; only in-tree Phase 1 primitives + Plan 02 `szs_validate` composed |

## Deviations from Plan

None — plan executed exactly as written. The `szs_validate.hpp` include was placed before the `#ifdef __SWITCH__` guard (consistent with the other neutral headers already at lines 1-4) rather than inside the guard, which is the correct pattern for this codebase per D-08/Pitfall 4 but the exact placement is consistent with what the plan's PATTERNS.md documented as "neutral header".

## Known Stubs

None — no stub values, placeholder text, or unwired data sources were introduced.

## Threat Flags

None — no new network endpoints, auth paths, file access patterns, or schema changes beyond what the plan's threat model already accounts for.

## Self-Check

### Created files exist:
- `source/platform/themes/nca_extract_switch.cpp` — modified (exists)
- `source/platform/themes/firmware_extract_switch.cpp` — modified (exists)

### Commits exist:
- `1785033` — feat(02-03): widen nca_romfs_filter to /lyt/ directory-prefix match (D-01)
- `d5bfc11` — feat(02-03): implement extract_all_base_layouts() multi-title extraction driver

## Self-Check: PASSED
