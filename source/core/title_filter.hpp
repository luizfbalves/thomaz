#pragma once
#include <cstdint>
#include <set>
#include "platform/title.hpp"

namespace thomaz {
namespace core {

enum class TitleKind { Game, NonGame };

// Classify a title purely based on its NACP heuristic.
// Returns NonGame only when BOTH save_data_size == 0 AND startup_user_account == 0.
// That is the profile of a homebrew forwarder; any real game has at least one of them set.
TitleKind classify(const InstalledTitle& t);

// Resolve whether a title is effectively hidden, ignoring the global show_hidden toggle.
// Priority:
//   1. id in force_shown  → false (explicitly visible)
//   2. id in force_hidden → true  (explicitly hidden)
//   3. otherwise          → classify(t) == NonGame
bool effectively_hidden(const InstalledTitle& t,
                        const std::set<std::uint64_t>& force_hidden,
                        const std::set<std::uint64_t>& force_shown);

} // namespace core
} // namespace thomaz
