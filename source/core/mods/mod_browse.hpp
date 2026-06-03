#pragma once
#include "core/cheat_repository.hpp" // UrlFetcher
#include "core/mods/gamebanana_types.hpp"
#include <cstdint>
#include <string>

namespace thomaz::core {

enum class BrowseStatus { Ok, NetworkError };
struct BrowseResult {
    BrowseStatus status = BrowseStatus::NetworkError;
    SearchPage page;
};

// Free-text mod search via apiv11. game_id==0 => global. Pure: fetcher injected.
BrowseResult search_mods(const std::string& query, std::uint64_t game_id, int page,
                         const UrlFetcher& fetch);

enum class ResolveStatus { Ok, NotFound, NetworkError };
struct ResolveResult {
    ResolveStatus status = ResolveStatus::NetworkError;
    std::vector<ModFile> files;
    std::string error;
};

// Resolve a mod's downloadable files (second request). NotFound when the API
// returns an _sErrorCode body; NetworkError on transport failure.
ResolveResult resolve_mod_files(std::uint64_t mod_id, const UrlFetcher& fetch);

} // namespace thomaz::core
