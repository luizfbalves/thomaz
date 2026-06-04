/*
    thomaz — fake title service implementation (desktop only).
*/

#ifndef __SWITCH__

#include "platform/title_service_fake.hpp"

namespace thomaz {

std::vector<InstalledTitle> FakeTitleService::listInstalled()
{
    // Desktop-only sample data. Icons are intentionally left empty so the
    // left-panel placeholder (violet square + initial) path is exercised.
    auto mk = [](std::uint64_t id, const char* name, const char* author,
                 std::uint32_t version, const char* dv, std::uint64_t saveSize) {
        InstalledTitle t;
        t.title_id        = id;
        t.name            = name;
        t.author          = author;
        t.version         = version;
        t.display_version = dv;
        t.save_data_size  = saveSize;
        return t;
    };
    return {
        mk(0x0100000000010000ULL, "Super Mario Odyssey",                "Nintendo", 393216,  "1.3.0", 32ULL  * 1024 * 1024),
        mk(0x01006A800016E000ULL, "Super Smash Bros. Ultimate",         "Nintendo", 1966080, "13.0.1", 96ULL * 1024 * 1024),
        mk(0x01007EF00011E000ULL, "The Legend of Zelda: Breath of the Wild", "Nintendo", 0,   "1.6.0", 0ULL),
        mk(0x0100F2C0115B6000ULL, "Mario Kart 8 Deluxe",                "Nintendo", 131072,  "3.0.3", 16ULL * 1024 * 1024),
        mk(0x01004D300C5AE000ULL, "Animal Crossing: New Horizons",      "Nintendo", 786432,  "2.0.6", 64ULL * 1024 * 1024),
    };
}

} // namespace thomaz

#endif // !__SWITCH__
