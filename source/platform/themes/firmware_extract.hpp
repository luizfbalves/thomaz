#pragma once
// Platform-neutral entry point for on-device firmware layout extraction.
//
// Composes the privileged extraction chain (key_loader + nca_extract + cfw_paths)
// into a single user-facing operation. This is the ONLY thomaz-facing symbol
// for firmware extraction; all platform knowledge is confined to the
// _switch.cpp / _fake.cpp pair (D-08 / Pitfall 4).
//
// Source lineage: exelix11/SwitchThemeInjector @ 2618b0c (GPLv2; thomaz is GPLv2)

#include <string>
#include <vector>

namespace thomaz {

// ExtractResult — return type of extract_base_layout().
//   ok    — true only if the szs was successfully validated and written to the
//            canonical /themes/systemData/<szs> path (D-03).
//   error — human-readable error or "relaunch" prompt; non-empty only on failure.
struct ExtractResult {
    bool        ok;
    std::string error;
};

// extract_base_layout — Extract a single firmware base layout to the canonical
// /themes/systemData/<szs> path so that base_present_for({target}) returns true.
//
// Parameters:
//   target — Themezer target name, e.g. "ResidentMenu". Must be a key
//             recognised by cfw_paths::target_map(). Unknown targets return
//             an error immediately.
//
// Switch behaviour (real impl, firmware_extract_switch.cpp):
//   (1) Applet gate first (TAKEOVER-01 / Pitfall 3): if appletGetAppletType()
//       != AppletType_Application, returns {false, "relaunch" message} before
//       any service init or fsOpenBisFileSystem call.
//   (2) Opens the privileged chain (pmdmnt/spl/splCrypto/BIS/lr) via
//       key_loader_switch — resolves the NCA path + derives the 32-byte header key.
//   (3) Calls nca_extract_switch to decrypt the target's RomFS in-memory
//       (NCA extraction fork) and capture the /lyt/<szs> buffer.
//   (4) Validates: non-empty buffer + SARC/Yaz0 szs magic (Pitfall 2 / T-01-15).
//   (5) Writes to cfw_paths::base_szs_path(target) (D-03 flat layout / Pitfall 6).
//   (6) Tears down all services on every exit path.
//   (7) Captures setsysGetFirmwareVersion for plan-05 provenance (D-07).
//
// Desktop behaviour (fake, firmware_extract_fake.cpp):
//   Returns {false, "Firmware extraction is only available on Switch."} — zero
//   Switch-specific symbols (D-08).
//
// Return:
//   {true, ""} on success (szs written, base_present_for flips true).
//   {false, <human-readable message>} on any failure.
ExtractResult extract_base_layout(const std::string& target);

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
//
// Desktop behaviour (firmware_extract_fake.cpp):
//   Returns {false, "Firmware extraction is only available on Switch.", {}, {}}
//   with zero Switch-specific symbols (D-08).
ExtractAllResult extract_all_base_layouts();

} // namespace thomaz
