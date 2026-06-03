#pragma once
#include "core/build_id.hpp"
#include "core/cheat.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

// Resolved cheats for one installed game, ready to display + persist.
struct CheatSet {
    Resolution resolution;            // which build_id, and how it was chosen
    std::vector<Cheat> cheats;        // cheats for resolution.build_id (master + regular)
    std::string sd_path;              // Atmosphere SD path to write enabled cheats to
    std::string title;                // game title from the versions db (may be empty)
};

enum class FetchStatus {
    Ok,           // cheats found and resolved (set is populated)
    NotInDb,      // db reachable but no cheats map to this game/version
    NetworkError  // a required document could not be fetched
};

struct FetchResult {
    FetchStatus status = FetchStatus::NetworkError;
    CheatSet set;
};

// Fetches a URL. Returns the body on success; an empty string when the server
// was reached but has no such document (e.g. HTTP 404 — game not in the db);
// and nullopt ONLY on a transport/connection failure (so NetworkError is
// reserved for genuine "can't reach the network", not "game has no cheats").
using UrlFetcher = std::function<std::optional<std::string>(const std::string& url)>;

// Orchestrate: fetch versions+cheats JSON, resolve the build_id for `version`,
// parse its cheats, and compute the SD path. Pure (no libcurl) — the fetcher is
// injected so this is host-testable.
FetchResult fetch_cheat_set(std::uint64_t title_id,
                            std::uint32_t version,
                            const UrlFetcher& fetch);

} // namespace thomaz::core
