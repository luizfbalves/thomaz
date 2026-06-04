#pragma once
#include "core/mods/gamebanana_types.hpp"
#include <string>

namespace thomaz::core {

// Parse an apiv11 search/listing response ({_aMetadata, _aRecords}). On
// malformed input returns an empty SearchPage (records empty, total 0).
SearchPage parse_search_page(const std::string& json);

struct ModFilesResult {
    bool ok = false;
    std::string error;            // _sErrorMessage when the body is an error
    std::vector<ModFile> files;
};

// Parse an apiv11 Mod/{id}?_csvProperties=_aFiles response. If the body carries
// an _sErrorCode (HTTP 200 error) or is malformed, returns ok=false.
ModFilesResult parse_mod_files(const std::string& json);

} // namespace thomaz::core
