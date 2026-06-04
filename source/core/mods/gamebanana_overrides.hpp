#pragma once
#include <cstdint>
#include <optional>

namespace thomaz::core {

// Static title_id -> GameBanana game_id override, for games whose GameBanana
// page name doesn't match the NACP name (or to skip name resolution entirely).
// Returns nullopt when the title isn't in the table (caller then resolves by
// name via apiv11). Extend kOverrides as (title_id, game_id) pairs are verified.
std::optional<std::uint64_t> gamebanana_game_override(std::uint64_t title_id);

} // namespace thomaz::core
