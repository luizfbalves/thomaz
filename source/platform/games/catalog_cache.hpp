#pragma once

#include "core/games/source_link.hpp"

#include <optional>
#include <string>

namespace thomaz {

std::string cache_path(const thomaz::core::SourceConfig& cfg);
std::optional<std::string> read_cached_index(const thomaz::core::SourceConfig& cfg);
bool write_cached_index(const thomaz::core::SourceConfig& cfg, const std::string& body);

} // namespace thomaz
