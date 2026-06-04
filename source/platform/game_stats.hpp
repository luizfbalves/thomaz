#pragma once
#include <cstdint>

namespace thomaz {

// Per-title play statistics, queried lazily on a detail screen (not during the
// installed-titles listing, to keep that fast). On Switch this comes from
// pdm:qry; on desktop builds a deterministic fake is returned.
struct GameStats {
    bool          valid        = false; // false = could not query / no record
    std::uint32_t play_minutes = 0;     // total play time in minutes
    std::uint32_t launches     = 0;     // total times the game was launched
    std::uint64_t last_played  = 0;     // POSIX seconds of last play, 0 = unknown
};

GameStats query_game_stats(std::uint64_t title_id);

} // namespace thomaz
