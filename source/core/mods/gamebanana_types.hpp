#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

// One mod as returned by an apiv11 search/listing record (_aRecords[]).
struct ModRecord {
    std::uint64_t id = 0;        // _idRow
    std::string name;            // _sName
    std::string model;           // _sModelName ("Mod", "Wip", ...)
    std::string profile_url;     // _sProfileUrl
    bool has_files = false;      // _bHasFiles
    std::uint32_t likes = 0;     // _nLikeCount (optional in JSON -> 0)
    std::uint32_t views = 0;     // _nViewCount (optional in JSON -> 0)
};

// One page of search/listing results (apiv11 {_aMetadata, _aRecords}).
struct SearchPage {
    std::vector<ModRecord> records;
    std::uint64_t total = 0;     // _aMetadata._nRecordCount
    int per_page = 0;            // _aMetadata._nPerpage (read from response)
    bool is_complete = true;     // _aMetadata._bIsComplete (false => more pages)
};

// One downloadable file of a mod (from _aFiles[]).
struct ModFile {
    std::uint64_t file_id = 0;   // _idRow
    std::string filename;        // _sFile
    std::uint64_t filesize = 0;  // _nFilesize (bytes)
    std::string md5;             // _sMd5Checksum
    std::string download_url;    // _sDownloadUrl (https://gamebanana.com/dl/<file_id>)
};

} // namespace thomaz::core
