#pragma once
#include <string>
#include <vector>

namespace thomaz::core {

// One entry as listed inside a mod archive (relative path, '/' separators).
struct ArchiveEntry {
    std::string path;        // e.g. "romfs/Actor/foo.bin" or "ModName/romfs/foo"
    bool is_dir = false;
};

enum class InstallError {
    None,        // plan is valid
    Empty,       // archive had no entries
    UnsafePath,  // an entry was absolute or contained a ".." segment
    NotRomfs,    // no "romfs/" directory segment found
    Ambiguous,   // entries live under more than one romfs root
};

// Result of analysing an archive's entry list.
struct InstallPlan {
    InstallError error = InstallError::Empty;
    // Prefix to strip from each entry so the remainder begins with "romfs/".
    // May be empty when entries already start at "romfs/".
    std::string strip_prefix;

    bool ok() const { return error == InstallError::None; }
};

// One mod folder present in a game's staging directory.
struct StagedMod {
    std::string name;        // folder name under the title's staging dir
    bool active = false;     // matches the title's active marker
};

} // namespace thomaz::core
