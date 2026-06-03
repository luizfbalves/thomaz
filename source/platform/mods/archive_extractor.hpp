#pragma once
#include "core/mods/mod_types.hpp"
#include <functional>
#include <string>
#include <vector>

namespace thomaz {

struct ExtractResult {
    bool ok = false;
    std::string error;       // human-readable on failure
    int files_written = 0;
};

// Reads the entry list of an archive without extracting (for plan_install).
// Returns an empty vector on read failure.
std::vector<core::ArchiveEntry> list_archive_entries(const std::string& archive_path);

// Extracts `archive_path` into `dest_dir`, stripping `strip_prefix` from each
// entry path so files land directly under dest_dir (dest_dir is expected to be
// the per-mod staging dir; entries then begin at "romfs/..."). Skips entries
// that fail the zip-slip guard. `progress` is called with (done, total) entry
// counts; total may be 0 if unknown.
ExtractResult extract_archive(const std::string& archive_path,
                              const std::string& dest_dir,
                              const std::string& strip_prefix,
                              const std::function<void(int, int)>& progress);

} // namespace thomaz
