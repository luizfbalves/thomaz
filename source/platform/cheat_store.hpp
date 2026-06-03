#pragma once
#include <optional>
#include <string>

namespace thomaz {

// Read a whole text file; nullopt if it does not exist or cannot be read.
// On Switch this reads the SD card via the standard POSIX fs.
std::optional<std::string> read_text_file(const std::string& path);

// Write `body` to `path`, creating any missing parent directories
// (e.g. /atmosphere/contents/<tid>/cheats/). Returns false on failure.
bool write_text_file(const std::string& path, const std::string& body);

// Where the cached switch-cheats-db index (versions.json) lives. Platform-
// specific: SD on Switch, a working-dir folder on desktop.
std::string index_cache_path();

// True if `dir` exists and contains at least one non-empty .txt file. Used for
// the "cheats active" badge (Atmosphère applies any cheat .txt present).
bool dir_has_nonempty_txt(const std::string& dir);

// Delete every .txt file directly in `dir` (the title's cheats folder), leaving
// the folder itself. Returns the number of files removed. Only touches *.txt.
int clear_cheat_files(const std::string& dir);

} // namespace thomaz
