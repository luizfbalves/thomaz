#pragma once
#include <atomic>
#include <memory>
#include <string>

namespace thomaz {

// Remember how the app was launched (argv[0]); call once from main().
void set_self_path(const char* argv0);

// Path of the .nro to overwrite when updating: the launch path if it looks like
// an .nro, otherwise the canonical /switch/thomaz.nro.
std::string update_target_path();

// Download a new .nro from `url` and atomically install it at `target`:
// streams to "<target>.tmp", then renames over `target`, so a failed or partial
// download never corrupts the running .nro. Returns false (and sets *err) on
// failure; the temp file is removed on any failure path.
//
// `cancelled` (optional, default null): cooperative abort flag forwarded to
// download_file's XFERINFOFUNCTION; when the owning activity is torn down the
// in-flight transfer aborts instead of running to completion (CONC-03, WR-01).
bool apply_downloaded_update(const std::string& url, const std::string& target,
                             std::string* err,
                             std::shared_ptr<std::atomic<bool>> cancelled = nullptr);

} // namespace thomaz
