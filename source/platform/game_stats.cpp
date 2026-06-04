/*
    thomaz — per-title play statistics.

    Switch: pdm:qry (PdmPlayStatistics). Desktop: deterministic fake so the
    two-column game panel renders fully during UI iteration.
*/

#include "platform/game_stats.hpp"

#ifdef __SWITCH__

#include <switch.h>

namespace thomaz {

GameStats query_game_stats(std::uint64_t title_id)
{
    GameStats g;
    if (R_FAILED(pdmqryInitialize()))
        return g;

    PdmPlayStatistics stats = {};
    // `true` aggregates across all user accounts on the console.
    Result rc = pdmqryQueryPlayStatisticsByApplicationId(title_id, true, &stats);
    pdmqryExit();

    if (R_FAILED(rc))
        return g;

    g.valid        = true;
    g.play_minutes = (std::uint32_t)(stats.playtime / 60000000000ULL); // ns -> minutes
    g.launches     = stats.total_launches;
    g.last_played  = stats.last_timestamp_user;
    return g;
}

} // namespace thomaz

#else // desktop

namespace thomaz {

GameStats query_game_stats(std::uint64_t title_id)
{
    // Deterministic fake values, varied a little by title id, for layout testing.
    GameStats g;
    g.valid        = true;
    g.play_minutes = 1287 + (std::uint32_t)(title_id & 0xFF);   // ~21h
    g.launches     = 42 + (std::uint32_t)((title_id >> 8) & 0x3F);
    g.last_played  = 1717428000ULL; // 2024-06-03, fixed for stable rendering
    return g;
}

} // namespace thomaz

#endif
