/*
    thomaz — cheat detail activity.
    For one installed game: downloads its cheats from switch-cheats-db, resolves
    the build_id for the installed version, lists the cheats as toggles, and
    writes the enabled set to the Atmosphère SD path (applied on next boot).
*/

#pragma once

#include <atomic>
#include <borealis.hpp>
#include <memory>
#include <utility>
#include <vector>

#include "core/cheat_repository.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

namespace thomaz {

class CheatDetailActivity : public brls::Activity
{
  public:
    CheatDetailActivity(InstalledTitle title, IHttpClient* http);
    ~CheatDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/cheat_detail.xml");

    void onContentAvailable() override;

  private:
    // Populate the screen from a fetch result (runs on the UI thread).
    void populate(const core::FetchResult& result);
    void save();

    InstalledTitle title;
    IHttpClient* http;
    core::CheatSet cheatSet;
    bool loaded = false;

    // Set false in the destructor so a still-running async fetch's UI callback
    // bails out instead of touching a destroyed activity.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);

    // (cheat name, its toggle) for every non-master cheat shown.
    std::vector<std::pair<std::string, brls::BooleanCell*>> toggles;
};

} // namespace thomaz
