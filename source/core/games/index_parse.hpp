#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

struct IndexFile {
    std::string url;
    std::uint64_t size         = 0;
    std::string nameOverride;
};

struct ParsedIndex {
    std::vector<IndexFile>   files;
    std::vector<std::string> directories;
    std::string              motd;
    bool                     ok = false;
};

ParsedIndex parse_index(const std::string& json);

} // namespace thomaz::core
