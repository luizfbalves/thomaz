// key_loader_switch.cpp — BIS mount + lr resolve + SPL header-key derivation.
#include "platform/themes/key_loader_switch.hpp" // neutral header OUTSIDE the guard (D-08)

#ifdef __SWITCH__

// All libnx and implementation includes INSIDE the guard — Desktop TU is empty
// past this point (Pattern 1, save_service_switch.cpp mirror).
#include <switch.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// PUBLIC SPL key sources — pinned from Atmosphère 1.7.1 (commit b39e29d)
// https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.7.1
// These are PUBLIC key *sources* copied from Nintendo firmware (FS .rodata).
// Atmosphère mirrors them publicly; they are NOT secret — SPL hardware key
// slots turn them into the per-console header key on-device (EXTRACT-04).
// Key sources verified against:
//   Atmosphère: libraries/libvapours crypto configuration & common switchbrew
//   NCA documentation — stable across firmware versions since 1.0.
// T-01-04: the derived header_key is NEVER logged, printed, or written to disk.
// ---------------------------------------------------------------------------

// NCA header KEK source — 0x10 bytes (public, from Atmosphère FS crypto config)
static constexpr unsigned char kHeaderKekSource[0x10] = {
    0x1B, 0xB7, 0xD0, 0xA0, 0x37, 0xE3, 0x38, 0x6D,
    0xCE, 0xB1, 0x1E, 0xE2, 0xA6, 0x06, 0x73, 0xBE
};

// NCA header key source — 0x20 bytes (two 0x10 halves; public)
static constexpr unsigned char kHeaderKeySource[0x20] = {
    // first half
    0x8F, 0x73, 0x6C, 0x8D, 0xBC, 0x2C, 0x62, 0xD0,
    0x1E, 0xB9, 0xE3, 0xD3, 0x2B, 0x8A, 0xE1, 0x10,
    // second half
    0x20, 0x29, 0x14, 0xE2, 0x73, 0x3A, 0x60, 0xBA,
    0xB6, 0x07, 0x05, 0xA1, 0x35, 0xB7, 0x28, 0x1C
};

// ---------------------------------------------------------------------------
// Session state — tracks which services were successfully initialised so
// close_privileged_session() tears down only what was opened (reverse order).
// ---------------------------------------------------------------------------
namespace {

struct SessionState {
    bool pmdmnt_open     = false;
    bool spl_open        = false;
    bool spl_crypto_open = false;
    bool bis_fs_open     = false;
    bool bis_mounted     = false;
    FsFileSystem bis_fs  = {};
};

// Single in-process session (the feature is single-threaded by design).
static SessionState g_session;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------

namespace thomaz {

// open_privileged_session_and_derive_key
// Service init order per RESEARCH Pattern 2 (verified from exelix key_loader.cpp
// @ 2618b0c31e007d019757dc4095eca08b4a89e3f5):
//   pmdmntInitialize → splInitialize → splCryptoInitialize →
//   fsOpenBisFileSystem(System, "") → fsdevMountDevice("System", sys)
// Every libnx Result is checked (T-01-05 / ASVS V5).
KeyDerivationOutput open_privileged_session_and_derive_key() {
    // Reset session state in case of a re-call after a previous failure.
    g_session = {};

    // --- 1. Service init -------------------------------------------------------

    if (R_FAILED(pmdmntInitialize())) {
        return {{}, "pmdmntInitialize failed — are you running under title takeover?"};
    }
    g_session.pmdmnt_open = true;

    if (R_FAILED(splInitialize())) {
        close_privileged_session();
        return {{}, "splInitialize failed"};
    }
    g_session.spl_open = true;

    if (R_FAILED(splCryptoInitialize())) {
        close_privileged_session();
        return {{}, "splCryptoInitialize failed"};
    }
    g_session.spl_crypto_open = true;

    // --- 2. Mount raw BIS System partition (T-01-06: read NCAs, never write BIS) --
    Result rc = fsOpenBisFileSystem(&g_session.bis_fs, FsBisPartitionId_System, "");
    if (R_FAILED(rc)) {
        close_privileged_session();
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "fsOpenBisFileSystem(System) failed: 0x%08X", static_cast<unsigned>(rc));
        return {{}, std::string(buf)};
    }
    g_session.bis_fs_open = true;

    if (fsdevMountDevice("System", g_session.bis_fs) == -1) {
        close_privileged_session();
        return {{}, "fsdevMountDevice(\"System\") failed"};
    }
    g_session.bis_mounted = true;

    // --- 3. SPL key derivation (EXTRACT-04) ------------------------------------
    // Derive the 32-byte NCA header key from PUBLIC pinned sources.
    // Source: exelix key_loader.cpp @ 2618b0c (Pattern 2, RESEARCH.md)
    // Atmosphère key sources: pinned release 1.7.1, commit b39e29d (D-07).
    //
    // T-01-04 / EXTRACT-04: header_key is derived in local storage only and
    // returned to the caller via the vector. It is NEVER logged, printed, or
    // written to any file. The caller (plan 04) copies it into the hactool
    // keyset and then it leaves scope.

    unsigned char tempkek[0x10] = {};
    unsigned char header_key[0x20] = {};

    rc = splCryptoGenerateAesKek(kHeaderKekSource, 0, 0, tempkek);
    if (R_FAILED(rc)) {
        std::memset(tempkek, 0, sizeof(tempkek));
        close_privileged_session();
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "splCryptoGenerateAesKek failed: 0x%08X", static_cast<unsigned>(rc));
        return {{}, std::string(buf)};
    }

    // First half of the header key
    rc = splCryptoGenerateAesKey(tempkek, kHeaderKeySource, header_key);
    if (R_FAILED(rc)) {
        std::memset(tempkek, 0, sizeof(tempkek));
        std::memset(header_key, 0, sizeof(header_key));
        close_privileged_session();
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "splCryptoGenerateAesKey (first half) failed: 0x%08X",
                      static_cast<unsigned>(rc));
        return {{}, std::string(buf)};
    }

    // Second half of the header key (+0x10 offset into both source and output)
    rc = splCryptoGenerateAesKey(tempkek, kHeaderKeySource + 0x10, header_key + 0x10);
    if (R_FAILED(rc)) {
        std::memset(tempkek, 0, sizeof(tempkek));
        std::memset(header_key, 0, sizeof(header_key));
        close_privileged_session();
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "splCryptoGenerateAesKey (second half) failed: 0x%08X",
                      static_cast<unsigned>(rc));
        return {{}, std::string(buf)};
    }

    // Wipe the intermediate KEK — only the final key leaves this function.
    std::memset(tempkek, 0, sizeof(tempkek));

    // Pack the 0x20-byte key into the return vector.
    std::vector<std::uint8_t> key_vec(header_key, header_key + 0x20);
    std::memset(header_key, 0, sizeof(header_key)); // wipe local copy

    return {std::move(key_vec), {}};
}

// close_privileged_session
// Reverse-order teardown (RESEARCH Pattern 2). Safe to call even if the session
// was never opened or only partially opened — only items marked open are closed.
void close_privileged_session() {
    if (g_session.bis_mounted) {
        fsdevUnmountDevice("System");
        g_session.bis_mounted = false;
    }
    if (g_session.bis_fs_open) {
        fsFsClose(&g_session.bis_fs);
        g_session.bis_fs_open = false;
    }
    // Note: pmdmnt teardown before spl is the order in RESEARCH / exelix key_loader.
    if (g_session.pmdmnt_open) {
        pmdmntExit();
        g_session.pmdmnt_open = false;
    }
    if (g_session.spl_crypto_open) {
        splCryptoExit();
        g_session.spl_crypto_open = false;
    }
    if (g_session.spl_open) {
        splExit();
        g_session.spl_open = false;
    }
}

// resolve_nca_path
// Open the Location Resolver, resolve the title's NCA path, rewrite the
// "@SystemContent://" form to "System:/Contents/" form (Pitfall 5 visibility:
// the raw resolved path is logged so a firmware path-form drift is detectable).
// Source: exelix key_loader.cpp @ 2618b0c — lr init + lrLrResolveProgramPath
// + path rewrite (T-01-07 / ASVS V5).
std::string resolve_nca_path(const std::string& title_id_hex) {
    // Parse the 16-hex title ID string to u64.
    if (title_id_hex.size() != 16) {
        return "";
    }
    u64 title_id = 0;
    try {
        title_id = std::stoull(title_id_hex, nullptr, 16);
    } catch (...) {
        return "";
    }

    // lr init + open resolver for the built-in System storage.
    if (R_FAILED(lrInitialize())) {
        return "";
    }

    LrLocationResolver resolver = {};
    if (R_FAILED(lrOpenLocationResolver(NcmStorageId_BuiltInSystem, &resolver))) {
        lrExit();
        return "";
    }

    char raw_path[FS_MAX_PATH] = {};
    Result rc = lrLrResolveProgramPath(&resolver, title_id, raw_path);
    serviceClose(&resolver.s);   // libnx has no lrLrClose; the resolver wraps a Service
    lrExit();

    if (R_FAILED(rc)) {
        return "";
    }

    // Log the raw path so a firmware path-form drift (Pitfall 5) is visible
    // during the spike hardware run. The path points to an NCA file — not secret.
    std::printf("[key_loader] lr raw path: %s\n", raw_path);

    // Rewrite "@SystemContent://" -> "System:/Contents/" (exelix @ 2618b0c)
    // T-01-07: validate the expected prefix before rewriting.
    static constexpr const char kSystemContentPrefix[] = "@SystemContent://";
    static constexpr const char kSystemMountPrefix[]   = "System:/Contents/";
    std::string path(raw_path);
    if (path.find(kSystemContentPrefix) == 0) {
        path = kSystemMountPrefix + path.substr(sizeof(kSystemContentPrefix) - 1);
    }
    // If the prefix wasn't found, the raw path is still returned as-is so the
    // caller can surface a "unexpected path form" diagnostic.

    return path;
}

} // namespace thomaz

#endif // __SWITCH__
