#pragma once

#include "core/games/index_parse.hpp"
#include "core/games/source_link.hpp"

namespace thomaz {

// SD scan root for local .nsp/.nsz pseudo-source (SRC-03).
std::string local_source_dir();

constexpr const char* kLocalSourceUrl = "local://sd";

bool is_local_source(const thomaz::core::SourceConfig& cfg);

thomaz::core::SourceConfig make_local_peer_config();

// Walks local_source_dir() (bounded depth/entry count) and synthesizes IndexFile
// rows that flow through core::group_catalog like a remote Tinfoil index.
thomaz::core::ParsedIndex scan_local_files(bool* truncated = nullptr);

} // namespace thomaz
