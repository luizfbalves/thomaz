#pragma once

#include "platform/title.hpp"

namespace brls { class Activity; }

namespace thomaz {

// Fill the shared left-column "game panel" of a detail screen from a title.
//
// The activity's XML must provide these view IDs (all optional — missing ones
// are skipped, so screens can opt out of fields):
//   gameIcon            (brls::Image)  — real icon, shown when title.icon != empty
//   gameIconPlaceholder (brls::Box)    — violet square shown when there is no icon
//   gameIconLetter      (brls::Label)  — first letter of the name, inside placeholder
//   gamePanelName       (brls::Label)  — game name
//   metaPublisher / metaVersion / metaSaveSize / metaTitleId /
//   metaPlaytime / metaLastPlayed (brls::Label) — metadata values
//
// Play statistics (playtime / last played) are queried lazily via game_stats.
void populate_game_panel(brls::Activity* root, const InstalledTitle& title);

} // namespace thomaz
