/*
    thomaz — game list activity.
    Lists installed titles from an ITitleService with their icon, an "active"
    badge (a cheat file is present on the SD), and — once the switch-cheats-db
    index loads — a "has cheats" badge. Tapping a title opens its cheats.
*/

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <borealis.hpp>
#include "app/thomaz_activity.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class GameListActivity : public ThomazActivity
{
  public:
    // Which detail screen a game tap opens (and which cheats-specific UI to show).
    enum class Target { Cheats, Mods };

    GameListActivity(ITitleService* titleService, IHttpClient* http,
                     Target target = Target::Cheats);

    CONTENT_FROM_XML_RES("activity/game_list.xml");

    void onContentAvailable() override;

  private:
    // Build the game rows on the UI thread once listInstalled() returns from the
    // worker thread (hides the spinner; shows the empty state if there are none).
    void populate(const std::vector<InstalledTitle>& titles);

    // Download/parse the db index off-thread, then reveal the "has cheats"
    // badges for covered titles (guarded by `alive`).
    void loadCheatIndexAsync();

    ITitleService* titleService;
    IHttpClient* http;
    Target target;

    // (title_id, its hidden "has cheats" badge) to reveal once the index is in.
    std::vector<std::pair<std::uint64_t, brls::View*>> hasCheatBadges;
};

} // namespace thomaz
