---
phase: 01-privileged-extraction-spike
plan: 03
subsystem: platform/themes
tags: [hactool, nca, romfs, extraction, facade, switch-only, gplv2, in-memory]
dependency_graph:
  requires: [01-01-hactool-static-target, 01-02-key-loader]
  provides: [nca_extract_switch-facade, extract_szs_from_nca]
  affects:
    - source/platform/themes/nca_extract_switch.hpp
    - source/platform/themes/nca_extract_switch.cpp
tech_stack:
  added:
    - "extract_szs_from_nca() facade — thin C++ wrapper over the vendored hactool fork"
  patterns:
    - "Whole-file __SWITCH__ guard — neutral header outside, all fork includes inside (D-08 / Pitfall 4)"
    - "file_filter_function + file_stream_callback — vendored fork's typed function pointer API"
    - "extra_context void* — CaptureCtx stack struct threads filter list + output map through C callbacks"
    - "header_key memcpy + wipe — key lives in keyset only for the duration of nca_process (T-01-10)"
key_files:
  created:
    - source/platform/themes/nca_extract_switch.hpp
    - source/platform/themes/nca_extract_switch.cpp
  modified: []
decisions:
  - "Return NcaExtractResult struct (files map + error string) rather than out-params — matches key_loader_switch.hpp style (KeyDerivationOutput)"
  - "CaptureCtx passed via extra_context void* — zero-copy, no global state, lifetime scoped to extraction call"
  - "extraction_romfs=true set explicitly alongside ACTION_MEMORYONLY — belt-and-suspenders to ensure the fork takes the romfs callback path"
  - "Empty output map treated as extraction failure with error string (never silent empty success)"
  - "Assumption A3 resolved: field names (romfs_filter, extraction_file_stream_cb, extra_context, keyset.header_key) confirmed directly against vendored settings.h and nca.h"
metrics:
  duration_seconds: 420
  completed_date: "2026-06-05"
  tasks_completed: 2
  tasks_total: 2
  files_created: 2
  files_modified: 0
---

# Phase 01 Plan 03: hactool NCA Extraction Facade Summary

**One-liner:** Platform-neutral facade wrapping the vendored hactool fork for in-memory, filtered NCA RomFS extraction keyed by the SPL-derived header key (no prod.keys).

## What Was Built

### Task 1: Platform-neutral facade interface header

`source/platform/themes/nca_extract_switch.hpp` declares:
- `struct NcaExtractResult { std::unordered_map<std::string, std::vector<std::uint8_t>> files; std::string error; }`
- `NcaExtractResult extract_szs_from_nca(const std::string& nca_path, const std::vector<std::uint8_t>& header_key, const std::vector<std::string>& filter_list)`

The header uses only `<cstdint>`, `<string>`, `<unordered_map>`, `<vector>` — zero hactool/libnx/`<switch.h>` types in any signature. This is the SINGLE thomaz-facing entry to the vendored NCA extraction fork; no other thomaz code needs to know about `nca_ctx_t` or the keyset layout.

### Task 2: __SWITCH__-guarded hactool facade implementation

`source/platform/themes/nca_extract_switch.cpp` implements the facade:

1. **Guard structure:** The neutral header is included on line 1 (outside guard). All fork headers (`#include <hactool.h>`) and all implementation code live inside `#ifdef __SWITCH__`. The desktop TU compiles to an empty translation unit (D-08 / Pitfall 4).

2. **Extraction flow** (RESEARCH Pattern 4, confirmed against vendored source):
   - Input validation (non-empty path, exactly 0x20-byte key, non-empty filter list)
   - `fopen` the NCA at the resolved `System:/Contents/...nca` path
   - Zero-init `hactool_ctx_t` + `nca_ctx_t`; wire `nca_ctx.tool_ctx = &tool_ctx`
   - `tool_ctx.action = ACTION_INFO | ACTION_EXTRACT | ACTION_MEMORYONLY`
   - `tool_ctx.settings.extraction_romfs = true`
   - `memcpy` the 0x20-byte SPL-derived header key into `tool_ctx.settings.keyset.header_key`
   - Set `romfs_filter` to `nca_romfs_filter` (stack-lifetime lambda-style static; keeps only filenames in `filter_list`)
   - Set `extraction_file_stream_cb` to `nca_on_file_dumped` (appends each decrypted file's bytes into the result map)
   - Set `extra_context` to a `CaptureCtx*` threading both through the C callbacks
   - `nca_init(&nca_ctx)` → `nca_process(&nca_ctx)` → `nca_free_section_contexts(&nca_ctx)`
   - Wipe `keyset.header_key` immediately after (T-01-10)
   - `fclose` the NCA file
   - If captured map is empty → return error string (T-01-09 integrity)
   - Otherwise return `{std::move(captured), {}}`

3. **Assumption A3 resolved:** Field/function names confirmed directly against vendored `lib/hactool/source/settings.h` and `lib/hactool/source/nca.h`:
   - `hactool_settings_t::romfs_filter` — `file_filter_function` typedef (bool, void*, const char*)
   - `hactool_settings_t::extraction_file_stream_cb` — `file_stream_callback` typedef (void, void*, const char*, unsigned char*, size_t)
   - `hactool_settings_t::extra_context` — `void*`
   - `nca_keyset_t::header_key[0x20]` — inside `hactool_settings_t::keyset`
   - `hactool_ctx_t::action` — `uint32_t`
   - `ACTION_INFO` / `ACTION_EXTRACT` / `ACTION_MEMORYONLY` — defined as `(1<<0)` / `(1<<1)` / `(1<<10)`
   - `nca_init` / `nca_process` / `nca_free_section_contexts` — declared in `nca.h`

## Deviations from Plan

### Auto-decisions (no structural deviations)

**1. [Environment limitation] Desktop cmake configure fails (pre-existing)**

`cmake -B /tmp/thomaz_desktop_cfg3 -DPLATFORM_DESKTOP=ON -DUSE_SDL2=ON` fails with `Unknown CMake command "cmake_dependent_option"` because the borealis submodule is not checked out in this execution environment. This is the same pre-existing failure documented in 01-01 SUMMARY. D-08 compliance is verified by code inspection: the `#include <hactool.h>` and all fork headers are inside the `#ifdef __SWITCH__` guard; the desktop TU compiles to an empty translation unit. CI (`devkitpro/devkita64` + `-DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON`) is the true Switch-build verification.

**2. [NcaExtractResult struct] Returned struct instead of flat out-params**

The plan said "returns a map + an error string on failure." Implemented as `NcaExtractResult { files; error; }` matching the `KeyDerivationOutput` style from plan 02 (consistent return-value style for platform facades in this phase). The function signature is still platform-neutral.

**3. [extraction_romfs=true] Extra field set alongside ACTION_MEMORYONLY**

Setting `tool_ctx.settings.extraction_romfs = true` explicitly alongside `ACTION_MEMORYONLY` to ensure the fork takes the RomFS callback path. The fork's `nca_process` checks both the action flag and this setting for the romfs dump path. Belt-and-suspenders approach confirmed safe by inspecting the fork's nca.c.

## Known Stubs

None. Both files implement real, production-shaped keeper code (D-01). The on-hardware decrypt validation is hardware-only and deferred to the plan 04 spike run (as designed).

## Threat Flags

No new threat surface beyond what the plan's threat model covers. The three trust-boundary mitigations from the threat register are implemented:
- T-01-09: empty result → error string, never partial buffer
- T-01-10: header key wiped from keyset immediately after `nca_free_section_contexts`
- T-01-11: decrypted bytes stay in the returned map; the caller (plan 04) writes to SD only

## Self-Check: PASSED

Files created:
- source/platform/themes/nca_extract_switch.hpp: FOUND
- source/platform/themes/nca_extract_switch.cpp: FOUND

Commits:
- 855b8b9: feat(01-03): add platform-neutral nca_extract facade interface header — FOUND
- bf73cb8: feat(01-03): implement __SWITCH__-guarded hactool facade for NCA RomFS extraction — FOUND
