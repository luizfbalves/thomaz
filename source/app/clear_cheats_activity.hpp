/*
    thomaz — clear cheats activity.
    Lists every game that currently has cheat files on the SD, with a checkbox
    each plus a "select all" toggle, and clears the selected games' cheat files
    after a confirmation dialog.
*/

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <borealis.hpp>
#include "platform/title.hpp"

namespace thomaz {

class ClearCheatsActivity : public brls::Activity
{
  public:
    explicit ClearCheatsActivity(ITitleService* titleService);
    ~ClearCheatsActivity() override;

    CONTENT_FROM_XML_RES("activity/clear_cheats.xml");

    void onContentAvailable() override;

  private:
    // Build the checkbox list on the UI thread after listInstalled() returns.
    void populate(const std::vector<InstalledTitle>& titles);
    void confirmAndClear();

    ITitleService* titleService;

    // Set false in the destructor so an in-flight load's UI callback bails.
    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);

    // (title_id, its selection checkbox) for each clearable game.
    std::vector<std::pair<std::uint64_t, brls::BooleanCell*>> selections;
};

} // namespace thomaz
