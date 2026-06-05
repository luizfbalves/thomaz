#pragma once
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

// Recursively delete `dir` and everything under it. Returns true if `dir` no
// longer exists afterwards (including when it never existed).
bool remove_tree(const std::string& dir);

// Immediate child directory names of `dir` (not recursive). Empty if `dir` is
// missing. Skips "." and "..".
std::vector<std::string> list_subdirs(const std::string& dir);

// Read/write/clear a one-line marker file. read returns nullopt when missing.
std::optional<std::string> read_marker(const std::string& path);
bool write_marker(const std::string& path, const std::string& value);
bool clear_marker(const std::string& path);

} // namespace thomaz
