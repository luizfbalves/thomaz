#pragma once
#include "core/sysmod/sysmod_types.hpp"
#include <string>

namespace thomaz::core {

// Parse a sysmodule's toolbox.json. Never throws. `valid` is true only when the
// input parses to a JSON object; individual wrong-typed fields fall back to
// defaults (name="", requires_reboot=true) without invalidating the result.
ToolboxInfo parse_toolbox(const std::string& raw);

} // namespace thomaz::core
