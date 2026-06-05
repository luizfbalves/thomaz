#pragma once
#include <string>

namespace thomaz {

// mkdir -p for the directory portion of `path` (everything before the last '/').
// Walks each '/' boundary and creates the directory up to that point.
// Trailing slashes are handled correctly: ensure_parent_dirs("x/y/") creates x and x/y.
void ensure_parent_dirs(const std::string& path);

// Recursively copy `src_dir` into `dst_dir` (created if missing). Returns false
// and sets *err on the first failure. Existing files at the destination are
// overwritten.
bool copy_tree(const std::string& src_dir, const std::string& dst_dir, std::string* err);

} // namespace thomaz
