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

} // namespace thomaz
