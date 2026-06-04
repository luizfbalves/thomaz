#include "core/mods/gamebanana_overrides.hpp"

namespace thomaz::core {

namespace {
struct Entry {
    std::uint64_t title_id;
    std::uint64_t game_id;
};
// Only VERIFIED pairs. Known GameBanana game_ids whose Switch title_ids still
// need confirming before adding here: Splatoon=6170, Splatoon 2=6383,
// Splatoon 3=15056. (game_id 6384 is the platform hub, NOT a game.)
constexpr Entry kOverrides[] = {
    { 0x01007EF00011E000ULL, 6386ULL }, // The Legend of Zelda: Breath of the Wild
};
} // namespace

std::optional<std::uint64_t> gamebanana_game_override(std::uint64_t title_id) {
    for (const Entry& e : kOverrides)
        if (e.title_id == title_id)
            return e.game_id;
    return std::nullopt;
}

} // namespace thomaz::core
