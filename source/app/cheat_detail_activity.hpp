/*
    thomaz — cheat detail activity.
    For one installed game: downloads its cheats from switch-cheats-db, resolves
    the build_id for the installed version, lists the cheats as toggles, and
    writes the enabled set to the Atmosphère SD path (applied on next boot).
*/

#pragma once

#include <borealis.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "app/thomaz_activity.hpp"
#include "core/cheat_repository.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class CheatDetailActivity : public ThomazActivity
{
  public:
    CheatDetailActivity(InstalledTitle title, IHttpClient* http);

    CONTENT_FROM_XML_RES("activity/cheat_detail.xml");

    void onContentAvailable() override;

  private:
    // Populate the screen from a fetch result (runs on the UI thread).
    void populate(const core::FetchResult& result);
    // Write the enabled set to the SD card. When showApplyInfo is true (the
    // explicit "save and apply" action), surface a dialog explaining the game
    // must be relaunched; toggle auto-saves pass false and only flash a toast.
    void save(bool showApplyInfo = false);

    InstalledTitle title;
    IHttpClient* http;
    core::CheatSet cheatSet;
    bool loaded = false;

    // (cheat name, its toggle) for every non-master cheat shown.
    std::vector<std::pair<std::string, brls::BooleanCell*>> toggles;
};

} // namespace thomaz
