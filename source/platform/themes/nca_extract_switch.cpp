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
// Returns true (keep) if the filename is in our filter_list; false (skip)
// otherwise. Passed as settings.romfs_filter.
static bool nca_romfs_filter(void* context, const char* file_name) {
    if (!context || !file_name) return false;
    const auto* ctx = static_cast<const CaptureCtx*>(context);
    const auto& list = *ctx->filter_list;
    return std::find(list.begin(), list.end(), std::string(file_name)) != list.end();
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

    // --- Wire filter and dump callback ----------------------------------------
    // CaptureCtx lifetime covers nca_init through nca_free_section_contexts.
    std::unordered_map<std::string, std::vector<std::uint8_t>> captured;
    CaptureCtx cap_ctx{ &filter_list, &captured };

    tool_ctx.settings.romfs_filter              = nca_romfs_filter;
    tool_ctx.settings.extraction_file_stream_cb = nca_on_file_dumped;
    tool_ctx.settings.extra_context             = &cap_ctx;

    // --- Run the extraction ---------------------------------------------------
    // Source: exelix hactool.cpp @ 2618b0c — RESEARCH Pattern 4.
    // nca_init:               parses + decrypts the NCA header.
    // nca_process:            walks sections; for each RomFS entry that passes
    //                         romfs_filter, calls extraction_file_stream_cb with
    //                         the decrypted in-memory buffer.
    // nca_free_section_contexts: releases per-section allocations.
    nca_init(&nca_ctx);
    nca_process(&nca_ctx);
    nca_free_section_contexts(&nca_ctx);

    // --- Wipe the header key immediately (T-01-10) ----------------------------
    std::memset(tool_ctx.settings.keyset.header_key, 0, 0x20);

    std::fclose(nca_file);
    nca_file = nullptr;

    // --- Validate output (T-01-09 integrity) ----------------------------------
    // An empty map means the decrypt failed, the NCA had no RomFS, or the
    // filter matched nothing. Return a clear error rather than an empty success.
    if (captured.empty()) {
        return {{}, "extract_szs_from_nca: extraction returned no files — "
                    "decrypt may have failed or filter matched nothing in RomFS"};
    }

    return {std::move(captured), {}};
}

} // namespace thomaz

#endif // __SWITCH__
