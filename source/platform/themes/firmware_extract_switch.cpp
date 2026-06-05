#include "platform/themes/firmware_extract.hpp"
#include "platform/themes/key_loader_switch.hpp"
#include "platform/themes/nca_extract_switch.hpp"
#include "platform/themes/cfw_paths.hpp"
#include "platform/themes/szs_validate.hpp"

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
// extract_all_base_layouts — multi-title best-effort extraction driver (Plan 03)
//
// Extracts every /lyt/*.szs from qlaunch (0100000000001000), Psl
// (0100000000001007), and MyPage (0100000000001013) into the canonical flat
// /themes/systemData/ directory. Composes the Phase 1 primitives:
//   open_privileged_session_and_derive_key / close_privileged_session (once)
//   resolve_nca_path (per title)
//   extract_szs_from_nca with {"/lyt/"} prefix filter (D-01)
//   is_structurally_valid_szs (D-04, Yaz0+SARC structural check)
//   ensure_parent_dirs + write_file (D-03, overwrite-in-place)
//
// Systemic vs per-part contract (D-02 / D-02a):
//   ok=false  → applet gate, key derivation failure, or BIS mount failure.
//              written_parts is empty on hard abort.
//   ok=true   → session succeeded; individual part/title failures collected in
//              failed_parts; written_parts lists every path successfully written.
//
// T-02-07: applet gate is the FIRST statement, before any service init.
// T-02-08: every buffer validated via is_structurally_valid_szs before write.
// T-02-09: output path is always base_layout_dir() + "/" + basename(romfs_key).
// T-02-10: close_privileged_session() called on abort path AND once after loop.
// T-02-11: D-03 overwrite-in-place; best-effort means a missing optional title
//          does not abort and leaves already-written parts intact.
// -----------------------------------------------------------------------------
ExtractAllResult extract_all_base_layouts() {

    // -------------------------------------------------------------------------
    // (1) APPLET GATE FIRST (TAKEOVER-01 / T-02-07).
    //     Must run before any setsys/SPL/BIS init — no service handles opened yet.
    // -------------------------------------------------------------------------
    if (appletGetAppletType() != AppletType_Application) {
        return {false,
                "Relaunch thomaz via title takeover "
                "(hold R while opening a game) to extract.",
                {},
                {}};
    }

    // -------------------------------------------------------------------------
    // (2) Capture firmware version once — applies to the whole multi-title run.
    //     Mirrors the Phase 1 pattern (extract_base_layout lines 88-97).
    // -------------------------------------------------------------------------
    SetSysFirmwareVersion fw{};
    if (R_SUCCEEDED(setsysInitialize())) {
        setsysGetFirmwareVersion(&fw);
        setsysExit();
    }
    std::printf("[firmware_extract] extract_all_base_layouts firmware=%d.%d.%d\n",
                static_cast<int>(fw.major),
                static_cast<int>(fw.minor),
                static_cast<int>(fw.micro));

    // -------------------------------------------------------------------------
    // (3) Open privileged session ONCE before the title loop (D-02a / T-02-10).
    //     Key derivation failure is a systemic abort: close and return ok=false.
    // -------------------------------------------------------------------------
    KeyDerivationOutput kdo = open_privileged_session_and_derive_key();
    if (!kdo.error.empty()) {
        close_privileged_session();
        return {false, "Key derivation failed: " + kdo.error, {}, {}};
    }
    if (kdo.header_key.size() != 0x20) {
        close_privileged_session();
        return {false, "Key derivation returned unexpected key length", {}, {}};
    }

    // -------------------------------------------------------------------------
    // (4) Per-title best-effort loop (D-02).
    //     Three title-IDs in order: qlaunch, Psl, MyPage.
    //     Per-part failures push to failed_parts and continue — never abort.
    // -------------------------------------------------------------------------
    std::vector<std::string> failed_parts;
    std::vector<std::string> written_parts;

    // D-01 directory-prefix filter: captures every /lyt/*.szs in one NCA pass.
    const std::vector<std::string> lyt_filter = {"/lyt/"};

    static const char* const kTitleIds[] = {
        "0100000000001000",   // qlaunch (ResidentMenu/Entrance/Flaunch/Set/Notification/common)
        "0100000000001007",   // Psl (player-select applet)
        "0100000000001013",   // MyPage (user page)
    };
    static const std::size_t kTitleCount = sizeof(kTitleIds) / sizeof(kTitleIds[0]);

    for (std::size_t i = 0; i < kTitleCount; ++i) {
        const std::string title_id(kTitleIds[i]);

        // Resolve the NCA path for this title via lr.
        std::string nca_path = resolve_nca_path(title_id);
        if (nca_path.empty()) {
            failed_parts.push_back(title_id + ": NCA resolve failed");
            continue;
        }
        std::printf("[firmware_extract] title %s → %s\n",
                    title_id.c_str(), nca_path.c_str());

        // Extract every /lyt/*.szs from this title's NCA in-memory.
        NcaExtractResult res = extract_szs_from_nca(nca_path, kdo.header_key, lyt_filter);
        if (!res.error.empty()) {
            failed_parts.push_back(title_id + ": " + res.error);
            continue;
        }

        // Per-file: validate (D-04) then write flat (D-03).
        for (auto& [romfs_key, buf] : res.files) {

            // D-04: structural validation — Yaz0-decompress + SARC-unpack.
            // is_structurally_valid_szs is used here (not the Phase 1 magic-only
            // is_valid_szs). T-02-08: invalid buffers go to failed_parts, never
            // overwrite a good file on disk.
            if (!is_structurally_valid_szs(buf)) {
                failed_parts.push_back(romfs_key + ": invalid szs");
                continue;
            }

            // D-03: output path is base_layout_dir() + "/" + basename(romfs_key).
            // T-02-09: only the last path segment is used — no ".." can survive
            // rfind('/')+1 on a firmware-controlled "/lyt/NAME.szs" key.
            //
            // CR-01: the "/lyt/" prefix filter accepts EVERY file under /lyt/,
            // not just *.szs. The structural SARC check above is integrity, not
            // authorization — it does not stop a non-layout SARC file from being
            // written under a firmware-controlled name. Gate the write on a
            // ".szs" extension and a non-empty basename so only true layout
            // files reach base_layout_dir(). This engine is the trust boundary
            // for third-party NCA content, so the RomFS key shape is never
            // assumed well-formed.
            const std::size_t slash = romfs_key.rfind('/');
            const std::string base =
                (slash == std::string::npos) ? std::string()
                                             : romfs_key.substr(slash + 1);
            if (base.empty() ||
                base.size() < 4 ||
                base.compare(base.size() - 4, 4, ".szs") != 0) {
                failed_parts.push_back(romfs_key + ": not a .szs layout, skipped");
                continue;
            }
            const std::string out  = base_layout_dir() + "/" + base;

            ensure_parent_dirs(out);

            // write_file opens std::ios::trunc — overwrite-in-place (D-03).
            if (write_file(out, buf)) {
                std::printf("[firmware_extract] wrote %zu bytes to %s\n",
                            buf.size(), out.c_str());
                written_parts.push_back(out);
            } else {
                failed_parts.push_back(out + ": write failed");
            }
        }
    }

    // -------------------------------------------------------------------------
    // (5) Close privileged session EXACTLY ONCE after the loop (T-02-10).
    //     Never called inside the per-title loop (Pitfall 2 / D-02a).
    // -------------------------------------------------------------------------
    close_privileged_session();

    // ok=true with per-part warnings is the normal best-effort outcome.
    // ok=false is reserved for the systemic aborts in (1) and (3) only.
    return {true, {}, failed_parts, written_parts};
}

} // namespace thomaz

#endif // __SWITCH__
