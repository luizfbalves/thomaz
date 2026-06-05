#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

// Output of open_privileged_session_and_derive_key().
// On success, header_key holds exactly 0x20 bytes.
// On failure, header_key is empty and error carries a human-readable message.
struct KeyDerivationOutput {
    std::vector<std::uint8_t> header_key; // 0x20 bytes on success; empty on failure
    std::string error;                    // non-empty only on failure
};

// Open privileged services (pmdmnt, spl, splCrypto) + mount the raw BIS System
// partition, then derive the 32-byte NCA header key from PUBLIC pinned key
// sources via SPL (EXTRACT-04 — no prod.keys). Returns the derived key or an
// error string. Caller must call close_privileged_session() when done regardless
// of outcome (safe no-op if init only partially succeeded).
KeyDerivationOutput open_privileged_session_and_derive_key();

// Unmount BIS System and shut down all privileged services opened by
// open_privileged_session_and_derive_key(). Safe to call even if the session
// was never opened or only partially opened.
void close_privileged_session();

// Resolve a title's NCA path via the Location Resolver on the mounted
// System partition. title_id is a 16-hex string (e.g. "0100000000001000").
// Returns the rewritten "System:/Contents/...nca" path on success, or an
// empty string on failure. Requires the privileged session to be open.
std::string resolve_nca_path(const std::string& title_id);

} // namespace thomaz
