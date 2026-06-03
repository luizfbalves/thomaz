#pragma once
#include "core/cheat_db.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::core {

struct Resolution {
    enum class Source {
        ExactVersion,        // version maps to a build_id that has cheats
        FallbackOlderBuild,  // exact build_id has no cheats (or version unknown); used newest build_id that does
        NotInDb              // no version-mapped build_id has cheats
    };
    Source source = Source::NotInDb;
    std::string build_id;    // empty iff NotInDb
};

// Resolve which build_id's cheats to use for an installed game version.
// `available` is the set of build_ids that actually have cheats (build_ids_with_cheats()).
Resolution resolve_build_id(std::uint32_t version,
                            const VersionMap& versions,
                            const std::vector<std::string>& available);

} // namespace thomaz::core
