#include "core/mods/gamebanana_overrides.hpp"

#include "core/mods/mod_paths.hpp"     // mod_staging_root
#include "platform/cheat_store.hpp"    // read_text_file

#include <nlohmann/json.hpp>
#include <cstdlib>

using nlohmann::json;

namespace thomaz::core {

namespace {
struct Entry {
    std::uint64_t title_id;
    std::uint64_t game_id;
};
// Compiled-in, VERIFIED pairs. Anything not 100% confirmed (BOTH the Switch
// title_id AND the GameBanana game_id) belongs in the SD overrides file instead,
// NOT here — a wrong pair silently resolves a game to another game's mod page.
constexpr Entry kOverrides[] = {
    { 0x01007EF00011E000ULL, 6386ULL }, // The Legend of Zelda: Breath of the Wild
};

// Path of the user/community-editable overrides file on the SD card (desktop:
// working dir). Lets anyone pin a game -> GameBanana game_id deterministically
// without rebuilding. See docs/gamebanana-overrides.md for the format.
std::string sd_overrides_path() {
    return mod_staging_root() + "/overrides.json";
}

// Look up `title_id` in the SD overrides file, if present. Read fresh each call
// (one small read per browser open) so edits apply without an app restart, and
// so behaviour is order-independent for tests.
std::optional<std::uint64_t> sd_override(std::uint64_t title_id) {
    auto body = thomaz::read_text_file(sd_overrides_path());
    if (!body)
        return std::nullopt;

    json j = json::parse(*body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded())
        return std::nullopt;

    // Accept either {"overrides":[...]} or a bare [...] array.
    const json* arr = nullptr;
    if (j.is_object() && j.contains("overrides") && j["overrides"].is_array())
        arr = &j["overrides"];
    else if (j.is_array())
        arr = &j;
    if (!arr)
        return std::nullopt;

    for (const auto& e : *arr) {
        if (!e.is_object() || !e.contains("title_id") || !e.contains("game_id"))
            continue;
        if (!e["title_id"].is_string())
            continue;
        // title_id is a 16-hex string ("0100..."); game_id a number (or string).
        std::uint64_t tid =
            std::strtoull(e["title_id"].get<std::string>().c_str(), nullptr, 16);
        if (tid != title_id)
            continue;
        if (e["game_id"].is_number_unsigned())
            return e["game_id"].get<std::uint64_t>();
        if (e["game_id"].is_string())
            return std::strtoull(e["game_id"].get<std::string>().c_str(), nullptr, 10);
    }
    return std::nullopt;
}
} // namespace

std::optional<std::uint64_t> gamebanana_game_override(std::uint64_t title_id) {
    // Compiled, verified pairs win.
    for (const Entry& e : kOverrides)
        if (e.title_id == title_id)
            return e.game_id;
    // Otherwise consult the user/community SD overrides file.
    return sd_override(title_id);
}

} // namespace thomaz::core
