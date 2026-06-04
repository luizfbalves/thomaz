#pragma once
#include "core/sysmod/sysmod_types.hpp"
#include <vector>

namespace thomaz::core {

// Turn a raw /atmosphere/contents listing into the user-facing sysmodule list.
// Skips entries without an exefs.nsp (those are LayeredFS game mods, not
// sysmodules). Result is sorted by display name, case-insensitively.
std::vector<Sysmodule> build_sysmodule_list(const std::vector<RawSysmoduleEntry>& entries);

} // namespace thomaz::core
