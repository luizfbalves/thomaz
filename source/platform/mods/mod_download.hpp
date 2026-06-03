#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace thomaz {

// Stream `url` to `dest_path` (parent dirs created). `progress(done,total)` is
// called during transfer; total may be 0 if the server doesn't send a length.
// Follows redirects (the GameBanana /dl/<id> link 30x-redirects to the file).
// Returns false and sets *err on failure.
bool download_file(const std::string& url, const std::string& dest_path,
                   const std::function<void(std::uint64_t done, std::uint64_t total)>& progress,
                   std::string* err);

} // namespace thomaz
