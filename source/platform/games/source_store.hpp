#pragma once

#include "core/games/source_link.hpp"

#include <string>
#include <vector>

namespace thomaz {

std::vector<thomaz::core::SourceConfig> load_sources();
bool save_sources(const std::vector<thomaz::core::SourceConfig>& sources);
std::string sources_config_path();

} // namespace thomaz
