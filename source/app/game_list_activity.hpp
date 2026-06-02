/*
    thomaz — game list activity.
    Lists installed titles from an ITitleService with their icon, an "active"
    badge (a cheat file is present on the SD), and — once the switch-cheats-db
    index loads — a "has cheats" badge. Tapping a title opens its cheats.
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <borealis.hpp>
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class GameListActivity : public brls::Activity
{
  public:
    GameListActivity(ITitleService* titleService, IHttpClient* http);
    ~GameListActivity() override;

    CONTENT_FROM_XML_RES("activity/game_list.xml");

    void onContentAvailable() override;

  private:
    // Download/parse the db index off-thread, then reveal the "has cheats"
    // badges for covered titles (guarded by `alive`).
    void loadCheatIndexAsync();

    ITitleService* titleService;
    IHttpClient* http;

    // Set false in the destructor so an in-flight index load's UI callback bails.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);

    // (title_id, its hidden "has cheats" badge) to reveal once the index is in.
    std::vector<std::pair<std::uint64_t, brls::View*>> hasCheatBadges;
};

} // namespace thomaz
