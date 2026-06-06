#pragma once
// Platform-neutral entry point for on-device firmware layout extraction.
//
// Composes the privileged extraction chain (key_loader + nca_extract + cfw_paths)
// into a single user-facing operation. This is the ONLY thomaz-facing symbol
// for firmware extraction; all platform knowledge is confined to
// firmware_extract_switch.cpp (D-08 / Pitfall 4).
//
// Source lineage: exelix11/SwitchThemeInjector @ 2618b0c (GPLv2; thomaz is GPLv2)

#include <string>
#include <vector>

namespace thomaz {

// ExtractAllResult — return type of extract_all_base_layouts().
//
// Systemic vs per-part contract (D-02/D-02a):
//   ok=false means a systemic failure aborted the entire operation (applet
//   gate, BIS mount failure, or key derivation failure). In this case
//   written_parts is empty and systemic_error carries the reason.
//
//   ok=true means the session succeeded overall; individual part failures
//   (missing szs, bad decrypt, write error) are collected in failed_parts
//   while successfully written paths accumulate in written_parts.
//
//   Invariant: ok=false implies written_parts is empty.
struct ExtractAllResult {
    bool ok;                                  // false only on systemic abort (D-02a)
    std::string systemic_error;               // non-empty on hard abort
    std::vector<std::string> failed_parts;    // per-part failure messages (D-02)
    std::vector<std::string> written_parts;   // canonical paths written (D-03)
};

// extract_all_base_layouts — Extract all firmware base layouts for qlaunch,
// Psl and MyPage into the canonical /themes/systemData/ directory so that
// base_present_for() returns true for all known targets.
//
// Switch behaviour (firmware_extract_switch.cpp):
//   Opens one privileged session (BIS + SPL + lr), iterates all three title-IDs
//   (qlaunch 0100000000001000, Psl 0100000000001007, MyPage 0100000000001013),
//   extracts every /lyt/*.szs per title, validates and writes flat to
//   base_layout_dir(). Session is closed exactly once on all exit paths.
ExtractAllResult extract_all_base_layouts();

} // namespace thomaz
