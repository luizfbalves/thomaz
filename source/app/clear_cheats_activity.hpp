/*
    thomaz — clear cheats activity.
    Lists every game that currently has cheat files on the SD, with a checkbox
    each plus a "select all" toggle, and clears the selected games' cheat files
    after a confirmation dialog.
*/

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <borealis.hpp>
#include "platform/title.hpp"

namespace thomaz {

class ClearCheatsActivity : public brls::Activity
{
  public:
    explicit ClearCheatsActivity(ITitleService* titleService);

    CONTENT_FROM_XML_RES("activity/clear_cheats.xml");

    void onContentAvailable() override;

  private:
    void confirmAndClear();

    ITitleService* titleService;

    // (title_id, its selection checkbox) for each clearable game.
    std::vector<std::pair<std::uint64_t, brls::BooleanCell*>> selections;
};

} // namespace thomaz
