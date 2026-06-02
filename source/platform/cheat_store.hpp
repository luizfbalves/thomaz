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

} // namespace thomaz
