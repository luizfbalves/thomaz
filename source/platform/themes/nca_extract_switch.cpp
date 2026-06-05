#include "platform/themes/nca_extract_switch.hpp" // neutral header OUTSIDE the guard (D-08)
// Thin C++ facade over the vendored hactool fork — in-memory, filtered NCA RomFS extraction.
// Source lineage: exelix11/SwitchThemeInjector hactool.cpp @ 2618b0c (GPLv2)
// RESEARCH.md Pattern 4 — action flags, romfs_filter, extraction_file_stream_cb.
// Field/function names confirmed against vendored settings.h and nca.h (Assumption A3).

#ifdef __SWITCH__

// All library includes INSIDE the guard — desktop TU is empty past this point.
// Pattern 1 (save_service_switch.cpp mirror), D-08 / Pitfall 4.
#include <hactool.h>   // includes settings.h, nca.h, pki.h, types.h, etc.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <setjmp.h>
#include <sys/stat.h>
#include <unistd.h>   // dup/dup2/close/fileno — restore stderr after redirect (WR-01)

// hactool recovery hooks (lib/hactool/source/hactool_recover.c). hactool aborts
// the whole process via exit() on any parse/key error; arming this jmp_buf turns
// that exit() into a longjmp back to the setjmp() below so we report a clean
// error instead of crashing the app.
extern "C" jmp_buf      g_hactool_recover_jmp;
extern "C" volatile int g_hactool_recover_active;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

// Capture context passed through hactool's void* extra_context.
struct CaptureCtx {
    const std::vector<std::string>*                                   filter_list;
    std::unordered_map<std::string, std::vector<std::uint8_t>>*       out_files;
};

// RomFS filename filter — called by hactool for every file in the RomFS.
// Returns true (keep) if the filename matches an entry in filter_list; false
// (skip) otherwise. Passed as settings.romfs_filter.
//
// D-01 directory-prefix match: if an entry ends with '/' it is treated as a
// directory prefix — any file_name that starts with that prefix is accepted.
// Example: "/lyt/" matches "/lyt/common.szs", "/lyt/ResidentMenu.szs", etc.
//
// Exact-name fallback (backward compat): entries without a trailing '/' are
// matched by equality, preserving single-target callers that pass e.g.
// "/lyt/ResidentMenu.szs" directly.
//
// file_name is the leading-slash absolute RomFS path (e.g. "/lyt/common.szs")
// as passed by hactool (nca.c:1743 + filepath.c:69).
static bool nca_romfs_filter(void* context, const char* file_name) {
    if (!context || !file_name) return false;
    const auto* ctx = static_cast<const CaptureCtx*>(context);
    const auto& list = *ctx->filter_list;
    std::string name(file_name);
    for (const auto& entry : list) {
        if (!entry.empty() && entry.back() == '/') {
            // Directory-prefix match: name starts with the entry string.
            if (name.rfind(entry, 0) == 0) return true;
        } else {
            // Exact-name match (single-target backward compat).
            if (name == entry) return true;
        }
    }
    return false;
}

// Per-file dump callback — called by hactool for every file that passes the
// filter. Copies the in-memory buffer into the result map.
// Passed as settings.extraction_file_stream_cb.
// T-01-09: only valid (non-null) data reaches the output map.
static void nca_on_file_dumped(void*          context,
                               const char*    file_name,
                               unsigned char* file_data,
                               size_t         length) {
    if (!context || !file_name || !file_data || length == 0) return;
    auto* ctx = static_cast<CaptureCtx*>(context);
    auto& out = *ctx->out_files;
    std::string name(file_name);
    out[name] = std::vector<std::uint8_t>(file_data, file_data + length);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------
namespace thomaz {

// extract_szs_from_nca
//
// Implementation notes (Assumption A3 confirmed against vendored source):
//   - hactool_ctx_t:  fields file_type, file, settings, action  (settings.h)
//   - nca_ctx_t:      field tool_ctx (hactool_ctx_t*)           (nca.h)
//   - settings:       romfs_filter, extraction_file_stream_cb, extra_context
//   - keyset:         keyset.header_key[0x20]                   (settings.h)
//   - ACTION flags:   ACTION_INFO, ACTION_EXTRACT, ACTION_MEMORYONLY (settings.h)
//   - C functions:    nca_init, nca_process, nca_free_section_contexts  (nca.h)
//
// T-01-09 (integrity): On any error or empty result, return empty map + error.
//   Never return a partial/garbage buffer.
// T-01-10 (key secrecy): Header key lives in keyset only for the duration of
//   the call; wiped immediately after use; never logged, stored, or returned.
// T-01-13 (crash resilience): hactool is wrapped; empty output = failure.
NcaExtractResult extract_szs_from_nca(
    const std::string&               nca_path,
    const std::vector<std::uint8_t>& header_key,
    const std::vector<std::string>&  filter_list)
{
    // --- Input validation (ASVS V5) -------------------------------------------
    if (nca_path.empty()) {
        return {{}, "extract_szs_from_nca: nca_path is empty"};
    }
    if (header_key.size() != 0x20) {
        return {{}, "extract_szs_from_nca: header_key must be exactly 0x20 bytes"};
    }
    if (filter_list.empty()) {
        return {{}, "extract_szs_from_nca: filter_list is empty — nothing to extract"};
    }

    // --- Open the NCA file ----------------------------------------------------
    FILE* nca_file = std::fopen(nca_path.c_str(), "rb");
    if (!nca_file) {
        return {{}, "extract_szs_from_nca: failed to open NCA: " + nca_path};
    }

    // --- Set up hactool contexts -----------------------------------------------
    // Zero-initialise both contexts to avoid stale-field surprises.
    hactool_ctx_t tool_ctx;
    std::memset(&tool_ctx, 0, sizeof(tool_ctx));

    nca_ctx_t nca_ctx;
    std::memset(&nca_ctx, 0, sizeof(nca_ctx));

    // Wire the NCA context to the tool context.
    nca_ctx.file     = nca_file;
    nca_ctx.tool_ctx = &tool_ctx;

    // File type and action flags (RESEARCH Pattern 4, confirmed in settings.h).
    tool_ctx.file_type = FILETYPE_NCA;
    tool_ctx.file      = nca_file;
    tool_ctx.action    = ACTION_INFO | ACTION_EXTRACT | ACTION_MEMORYONLY;

    // Enable RomFS extraction path (required alongside ACTION_EXTRACT +
    // ACTION_MEMORYONLY for the romfs callback path in the fork).
    tool_ctx.settings.extraction_romfs = true;

    // --- Load the header key into the keyset ----------------------------------
    // The 0x20-byte SPL-derived key is memcpy'd into hactool's keyset for the
    // duration of this call only. Never logged, stored, or returned (T-01-10).
    std::memcpy(tool_ctx.settings.keyset.header_key,
                header_key.data(),
                0x20);

    // --- Load the PUBLIC key-area-key source (kaek 0 / application) ------------
    // hactool's nca_decrypt_key_area() (__SWITCH__ path) uses SPL to decrypt the
    // NCA key area into the per-section keys, seeded by THIS public source. The
    // header_key alone only decrypts the NCA *header*; without this source the
    // key area derives to zero -> section keys are wrong -> the RomFS parse walks
    // garbage offsets and the app crashes (data abort). This value is PUBLIC
    // (mirrored by Atmosphère, same provenance as the header sources in
    // key_loader_switch.cpp); kaek index 0 (application) covers the firmware
    // theme titles qlaunch/Psl/MyPage. (EXTRACT-04 — still no prod.keys.)
    static const unsigned char kKeyAreaKeyApplicationSource[0x10] = {
        0x7F, 0x59, 0x97, 0x1E, 0x62, 0x9F, 0x36, 0xA1,
        0x30, 0x98, 0x06, 0x6F, 0x21, 0x44, 0xC3, 0x0D
    };
    std::memcpy(tool_ctx.settings.keyset.key_area_key_application_source,
                kKeyAreaKeyApplicationSource, 0x10);

    // --- Wire filter and dump callback ----------------------------------------
    // CaptureCtx lifetime covers nca_init through nca_free_section_contexts.
    std::unordered_map<std::string, std::vector<std::uint8_t>> captured;
    CaptureCtx cap_ctx{ &filter_list, &captured };

    tool_ctx.settings.romfs_filter              = nca_romfs_filter;
    tool_ctx.settings.extraction_file_stream_cb = nca_on_file_dumped;
    tool_ctx.settings.extra_context             = &cap_ctx;

    // --- Capture hactool's stderr to an SD log (diagnostics) ------------------
    // hactool reports errors via fprintf(stderr) right before exit(); on Switch
    // stderr normally goes nowhere. Redirect it to a file so a failure (e.g.
    // "Invalid NCA header! Are keys correct?") is readable back on screen.
    const char* kErrLog = "/switch/thomaz/hactool.log";
    ::mkdir("/switch", 0777);
    ::mkdir("/switch/thomaz", 0777);
    std::fflush(stderr);
    // WR-01: save the original stderr fd so the redirect is LOCAL to this call.
    // Without restoring it, freopen rebinds process-wide stderr permanently and
    // every later fprintf/assert in the whole app goes to this log (truncated on
    // the next extraction). dup() the underlying fd now; dup2() it back below.
    const int saved_stderr_fd = ::dup(::fileno(stderr));
    // WR-06: freopen closes the original stream and returns NULL on failure.
    // If the redirect fails (e.g. /switch/thomaz not writable), proceed WITHOUT
    // redirection rather than writing every subsequent fprintf to a now-closed
    // stream. stderr_redirected gates the diagnostics below and the read-back.
    const bool stderr_redirected = (std::freopen(kErrLog, "w", stderr) != nullptr);

    // Build stamp (FIRST line of the log) — uniquely identifies which nro is on
    // the SD card. __DATE__/__TIME__ are the compile time of THIS TU, so a stale
    // install is obvious: compare this line to the nro's mtime. The "iso3" tag
    // marks the isolated-mbedtls build (hactool bound to our private non-PSA
    // mbedtls, not the portlib PSA copy that fails XTS decrypt setkey).
    // WR-06: only write to stderr if the redirect to kErrLog succeeded.
    if (stderr_redirected) {
        std::fprintf(stderr, "thomaz hactool build: %s %s [iso9-switchdef]\n", __DATE__, __TIME__);
        std::fflush(stderr);

    // --- Diagnostics (non-secret) to localize the "Invalid NCA header" cause --
    // The key VALUE is never printed (T-01-04); only whether it is all-zero (SPL
    // silent-failure signal) plus a non-invertible 2-byte XOR fold. The raw NCA
    // bytes peeked below are still ENCRYPTED on disk and are not secret.
        bool key_zero = true;
        std::uint16_t fold = 0;
        for (std::size_t i = 0; i < header_key.size(); ++i) {
            if (header_key[i] != 0) key_zero = false;
            fold ^= static_cast<std::uint16_t>(header_key[i] << ((i & 1) * 8));
        }
        std::fprintf(stderr, "diag: nca_path=%s\n", nca_path.c_str());
        std::fprintf(stderr, "diag: header_key all_zero=%d fold=%04x\n",
                     key_zero ? 1 : 0, fold);

        unsigned char peek[0x210];
        std::fseek(nca_file, 0, SEEK_END);
        long fsz = std::ftell(nca_file);
        std::fseek(nca_file, 0, SEEK_SET);
        std::size_t pn = std::fread(peek, 1, sizeof(peek), nca_file);
        std::fseek(nca_file, 0, SEEK_SET);   // hactool re-seeks to 0 anyway
        std::fprintf(stderr, "diag: file_size=%ld read=%zu\n", fsz, pn);
        std::fprintf(stderr, "diag: enc[0x000]=");
        for (int i = 0; i < 16 && static_cast<std::size_t>(i) < pn; ++i)
            std::fprintf(stderr, "%02x", peek[i]);
        std::fprintf(stderr, "\ndiag: enc[0x200]=");
        for (int i = 0; i < 16 && static_cast<std::size_t>(0x200 + i) < pn; ++i)
            std::fprintf(stderr, "%02x", peek[0x200 + i]);
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }

    // --- Run the extraction under a recovery guard ----------------------------
    // Source: exelix hactool.cpp @ 2618b0c — RESEARCH Pattern 4.
    // nca_init:    parses + decrypts the NCA header (exit()s on a bad key).
    // nca_process: walks sections; for each RomFS entry that passes romfs_filter,
    //              calls extraction_file_stream_cb with the decrypted buffer.
    // hactool's exit() on error longjmps back to the setjmp below (see
    // hactool_recover.*), so a wrong key surfaces as an error, not an app crash.
    // NOTE: do NOT call nca_init(&nca_ctx) here — in this fork nca_init() is just
    // `memset(ctx, 0, sizeof(*ctx))`, so calling it AFTER we set nca_ctx.file and
    // nca_ctx.tool_ctx would wipe them back to NULL and nca_process() would
    // fseeko64() a NULL FILE* (data abort, the on-hardware crash). We already
    // zero-initialised nca_ctx above, so the context is ready as-is.
    bool aborted = false;
    g_hactool_recover_active = 1;
    if (setjmp(g_hactool_recover_jmp) == 0) {
        nca_process(&nca_ctx);
        nca_free_section_contexts(&nca_ctx);
    } else {
        aborted = true;   // hactool called exit() — recovered here (sections leaked)
    }
    g_hactool_recover_active = 0;

    // --- Wipe the header key immediately (T-01-10) ----------------------------
    std::memset(tool_ctx.settings.keyset.header_key, 0, 0x20);

    std::fclose(nca_file);
    nca_file = nullptr;

    // Read back the captured hactool stderr (last chunk) for the error message.
    // WR-06: only meaningful if the redirect succeeded.
    std::fflush(stderr);
    std::string hactool_err;
    if (stderr_redirected) {
      if (FILE* ef = std::fopen(kErrLog, "rb")) {
        char ebuf[512];
        size_t n = std::fread(ebuf, 1, sizeof(ebuf) - 1, ef);
        ebuf[n] = '\0';
        std::fclose(ef);
        // Trim trailing whitespace/newlines for a tidy on-screen message.
        hactool_err.assign(ebuf, n);
        while (!hactool_err.empty() &&
               (hactool_err.back() == '\n' || hactool_err.back() == '\r' ||
                hactool_err.back() == ' '))
            hactool_err.pop_back();
      }
    }

    // WR-01: restore the original process-wide stderr. After this point stderr
    // points back at wherever it did on entry, so unrelated diagnostics for the
    // rest of the session are no longer swallowed by hactool.log.
    if (saved_stderr_fd >= 0) {
        std::fflush(stderr);
        ::dup2(saved_stderr_fd, ::fileno(stderr));
        ::close(saved_stderr_fd);
    }

    // --- hactool aborted (bad key, corrupt NCA, etc.) -------------------------
    if (aborted) {
        std::string msg = "hactool aborted during NCA decode";
        if (!hactool_err.empty()) msg += ": " + hactool_err;
        return {{}, msg};
    }

    // --- Validate output (T-01-09 integrity) ----------------------------------
    // An empty map means the decrypt failed, the NCA had no RomFS, or the
    // filter matched nothing. Return a clear error rather than an empty success.
    if (captured.empty()) {
        std::string msg = "extract_szs_from_nca: extraction returned no files — "
                          "decrypt may have failed or filter matched nothing in RomFS";
        if (!hactool_err.empty()) msg += " [" + hactool_err + "]";
        return {{}, msg};
    }

    return {std::move(captured), {}};
}

} // namespace thomaz

#endif // __SWITCH__
