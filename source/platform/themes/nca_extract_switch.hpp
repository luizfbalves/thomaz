#pragma once
// Platform-neutral facade interface for in-memory NCA RomFS extraction.
//
// This is the SINGLE thomaz-facing entry to the vendored NCA extraction fork.
// All C-API knowledge (keyset, ctx structs, etc.) is confined entirely to
// nca_extract_switch.cpp under a whole-file __SWITCH__ guard (D-08 / Pitfall 4).
//
// Source lineage: exelix11/SwitchThemeInjector @ 2618b0c (GPLv2)

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace thomaz {

// NcaExtractResult — return type of extract_szs_from_nca().
//   files — map of RomFS filename -> byte vector for each matched file.
//           Empty on failure or when the filter matched nothing.
//   error — human-readable error description; non-empty only on failure.
struct NcaExtractResult {
    std::unordered_map<std::string, std::vector<std::uint8_t>> files;
    std::string error;
};

// extract_szs_from_nca — Decrypt an NCA's RomFS in-memory using the supplied
// SPL-derived header key, apply the given filename filter, and return the
// matched files as byte buffers.
//
// Parameters:
//   nca_path    — Resolved NCA path in the form "System:/Contents/...nca"
//                 (from resolve_nca_path() in the key loader module).
//   header_key  — The 0x20-byte NCA header key derived on-device via SPL
//                 (from open_privileged_session_and_derive_key()). Must be
//                 exactly 0x20 bytes; if not, returns an empty map + error.
//                 No prod.keys involved (EXTRACT-04).
//   filter_list — Filenames to keep (RomFS-relative paths, e.g.
//                 "/lyt/ResidentMenu.szs"). Only matched files are captured.
//                 For the Phase 1 spike, pass a single-entry list.
//
// Returns:
//   On success — NcaExtractResult with a non-empty files map.
//   On any error, bad header-key length, file-open failure, or empty decrypt
//   result — empty map and a human-readable error string.
//   NEVER returns a partial or garbage buffer (T-01-09 integrity).
//
// Desktop build: returns an empty map + "NCA extraction is only available on
// Nintendo Switch." — the entire implementation is inside a __SWITCH__ guard.
NcaExtractResult extract_szs_from_nca(
    const std::string&               nca_path,
    const std::vector<std::uint8_t>& header_key,
    const std::vector<std::string>&  filter_list);

} // namespace thomaz
