#pragma once
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

// Recursively copy `src_dir` into `dst_dir` (created if missing). Returns false
// and sets *err on the first failure. Existing files at the destination are
// overwritten.
bool copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err);

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
