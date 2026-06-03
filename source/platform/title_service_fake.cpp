/*
    thomaz — fake title service implementation (desktop only).
*/

#ifndef __SWITCH__

#include "platform/title_service_fake.hpp"

namespace thomaz {

std::vector<InstalledTitle> FakeTitleService::listInstalled()
{
    return {
        { 0x0100000000010000ULL, "Super Mario Odyssey",               "Nintendo",         393216  },
        { 0x01006A800016E000ULL, "Super Smash Bros. Ultimate",        "Nintendo",         1966080 },
        { 0x01007EF00011E000ULL, "The Legend of Zelda: Breath of the Wild", "Nintendo",   0       },
        { 0x0100F2C0115B6000ULL, "Mario Kart 8 Deluxe",              "Nintendo",         131072  },
        { 0x01004D300C5AE000ULL, "Animal Crossing: New Horizons",    "Nintendo",         786432  },
    };
}

} // namespace thomaz

#endif // !__SWITCH__
