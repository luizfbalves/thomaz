#pragma once
#include "core/cheat.hpp"
#include <string>
#include <vector>
#include <set>

namespace thomaz::core {

// Parse an Atmosphère cheat .txt file body into ordered cheats.
// A header line is a trimmed line of the form [Name] (regular) or {Name} (master).
// Non-blank lines after a header are that cheat's opcode lines until the next header.
// Lines before the first header are ignored.
std::vector<Cheat> parse_txt(const std::string& content);

// Serialize "master cheats + only the enabled regular cheats" back to .txt body.
// Order is preserved from `cheats`. A cheat is included if it is_master OR its name is in enabled.
// Each cheat is written as: header line, then its opcode lines, then one blank separator line.
std::string serialize_txt(const std::vector<Cheat>& cheats, const std::set<std::string>& enabled);

// Names of the regular (non-master) cheats present in an existing cheat .txt body.
// Used to pre-check toggles from a previously-saved file. Master cheats are
// always applied, so they are excluded from this "user-enabled" set.
std::set<std::string> enabled_cheat_names(const std::string& content);

} // namespace thomaz::core
