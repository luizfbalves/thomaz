#include "platform/themes/firmware_extract.hpp"
#include "platform/themes/key_loader_switch.hpp"
#include "platform/themes/nca_extract_switch.hpp"
#include "platform/themes/cfw_paths.hpp"

#ifdef __SWITCH__

#include <switch.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

namespace thomaz {

namespace {

// SZS validation: accept SARC ("SARC" magic at offset 0) or Yaz0-compressed
// SZS ("Yaz0" magic at offset 0). Either constitutes a valid layout file.
// (Research Pitfall 2 / T-01-15 integrity threat.)
bool is_valid_szs(const std::vector<std::uint8_t>& buf) {
    if (buf.size() < 4) return false;
    // SARC magic: 0x53 0x41 0x52 0x43 = "SARC"
    if (buf[0] == 0x53 && buf[1] == 0x41 && buf[2] == 0x52 && buf[3] == 0x43) return true;
    // Yaz0 magic: 0x59 0x61 0x7A 0x30 = "Yaz0"
    if (buf[0] == 0x59 && buf[1] == 0x61 && buf[2] == 0x7A && buf[3] == 0x30) return true;
    return false;
}

// mkdir -p for the parent directories of a file path (POSIX/FAT-safe).
// Mirrors theme_install.cpp::ensure_parent_dirs (lines 38-44).
void ensure_parent_dirs(const std::string& file_path) {
    std::string acc;
    for (size_t i = 0; i < file_path.size(); ++i) {
        acc.push_back(file_path[i]);
        if (file_path[i] == '/' && acc.size() > 1) ::mkdir(acc.c_str(), 0777);
    }
}

// Binary truncated write. Mirrors theme_install.cpp::write_file (lines 30-35).
bool write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    if (!data.empty()) out.write(reinterpret_cast<const char*>(data.data()),
                                 static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

} // namespace

// -----------------------------------------------------------------------------
// extract_base_layout — the vertical-slice spike entry point (Plan 04)
// Chains: applet gate → key_loader (BIS/lr/SPL) → nca_extract (NCA fork) →
//         szs-magic validate → write to base_szs_path.
// Captures firmware version for plan-05 provenance (D-07).
// -----------------------------------------------------------------------------
ExtractResult extract_base_layout(const std::string& target) {

    // -------------------------------------------------------------------------
    // (1) APPLET GATE FIRST (TAKEOVER-01 / Research Pattern 3 / Pitfall 3).
    //     Must run before any service init or fsOpenBisFileSystem call.
    // -------------------------------------------------------------------------
    if (appletGetAppletType() != AppletType_Application) {
        return {false,
                "Relaunch thomaz via title takeover "
                "(hold R while opening a game) to extract."};
    }

    // -------------------------------------------------------------------------
    // (2) Resolve the target via cfw_paths::target_map.
    // -------------------------------------------------------------------------
    auto tm = cfw_paths::target_map(target);
    if (!tm) {
        return {false, "Unknown extraction target: " + target};
    }
    // The filter list for the NCA extraction: one RomFS path.
    std::vector<std::string> filter_list = {"/lyt/" + tm->szs};
    std::string out_path = cfw_paths::base_szs_path(target);
    if (out_path.empty()) {
        return {false, "Could not resolve output path for target: " + target};
    }

    // -------------------------------------------------------------------------
    // (7) Capture firmware version for plan-05 provenance (D-07).
    //     Init setsys early so it is always available for the log line.
    // -------------------------------------------------------------------------
    SetSysFirmwareVersion fw{};
    if (R_SUCCEEDED(setsysInitialize())) {
        setsysGetFirmwareVersion(&fw);
        setsysExit();
    }
    std::printf("[firmware_extract] target=%s firmware=%d.%d.%d\n",
                target.c_str(),
                static_cast<int>(fw.major),
                static_cast<int>(fw.minor),
                static_cast<int>(fw.micro));

    // -------------------------------------------------------------------------
    // (3) Open privileged session: pmdmnt/spl/splCrypto/BIS/lr.
    //     Derives the 32-byte NCA header key from PUBLIC pinned key sources
    //     (EXTRACT-04 — no prod.keys).
    // -------------------------------------------------------------------------
    KeyDerivationOutput kdo = open_privileged_session_and_derive_key();
    if (!kdo.error.empty()) {
        close_privileged_session();
        return {false, "Key derivation failed: " + kdo.error};
    }
    if (kdo.header_key.size() != 0x20) {
        close_privileged_session();
        return {false, "Key derivation returned unexpected key length"};
    }

    // -------------------------------------------------------------------------
    // Resolve NCA path for this title via lr.
    // -------------------------------------------------------------------------
    std::string nca_path = resolve_nca_path(tm->title_id);
    if (nca_path.empty()) {
        close_privileged_session();
        return {false, "Could not resolve NCA path for title " + tm->title_id};
    }
    std::printf("[firmware_extract] NCA path resolved: %s\n", nca_path.c_str());

    // -------------------------------------------------------------------------
    // (4) Extract the filtered SZS in-memory via the NCA extraction facade
    //     (nca_extract_switch — wraps the vendored NCA RomFS fork).
    // -------------------------------------------------------------------------
    NcaExtractResult nca_res = extract_szs_from_nca(nca_path, kdo.header_key, filter_list);

    // Tear down privileged services (reverse-order, always).
    close_privileged_session();

    if (!nca_res.error.empty()) {
        return {false, "NCA extraction failed: " + nca_res.error};
    }

    // -------------------------------------------------------------------------
    // (5) VALIDATE before writing (Pitfall 2 / T-01-15 integrity).
    //     Never overwrite a good file with empty/garbage bytes.
    // -------------------------------------------------------------------------
    const std::string romfs_key = "/lyt/" + tm->szs;
    auto it = nca_res.files.find(romfs_key);
    if (it == nca_res.files.end() || it->second.empty()) {
        return {false, "Extraction produced no output for " + romfs_key};
    }
    const std::vector<std::uint8_t>& szs_buf = it->second;
    if (!is_valid_szs(szs_buf)) {
        return {false, "Extracted buffer has invalid SZS magic (bad decrypt?)"};
    }

    // -------------------------------------------------------------------------
    // (6) Write to the canonical flat path (D-03 / Pitfall 6).
    //     Uses the ensure_parent_dirs + binary-trunc write_file pattern from
    //     theme_install.cpp. NOT the exelix extracted/{qlaunch}/ subdir.
    // -------------------------------------------------------------------------
    ensure_parent_dirs(out_path);
    if (!write_file(out_path, szs_buf)) {
        return {false, "Failed to write " + out_path};
    }

    std::printf("[firmware_extract] wrote %zu bytes to %s\n",
                szs_buf.size(), out_path.c_str());

    return {true, ""};
}

} // namespace thomaz

#endif // __SWITCH__
