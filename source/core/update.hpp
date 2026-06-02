#pragma once
#include <string>

namespace thomaz::core {

// Parsed from the GitHub "latest release" API response.
struct ReleaseInfo {
    std::string tag;      // e.g. "v0.2.0"
    std::string nro_url;  // browser_download_url of the matching .nro asset
    std::string notes;    // release body / changelog
    bool valid = false;   // true iff both tag and nro_url were found
};

// GitHub "latest release" API endpoint for the thomaz repo.
std::string github_latest_release_url();

// Extract the tag, release notes, and the download URL of the asset named
// `asset_name` (e.g. "thomaz.nro") from the GitHub latest-release JSON.
ReleaseInfo parse_latest_release(const std::string& github_json,
                                 const std::string& asset_name);

// True if `latest` is a newer version than `current`. Both may have a leading
// 'v'; components are compared numerically (missing trailing parts are 0).
bool is_newer_version(const std::string& latest, const std::string& current);

} // namespace thomaz::core
