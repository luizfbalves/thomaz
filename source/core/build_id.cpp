#include "core/build_id.hpp"
#include <algorithm>
#include <set>

namespace thomaz::core {

Resolution resolve_build_id(std::uint32_t version,
                            const VersionMap& versions,
                            const std::vector<std::string>& available) {
    const std::set<std::string> have(available.begin(), available.end());

    // 1. Exact version -> build_id that has cheats.
    auto exact = versions.by_version.find(version);
    if (exact != versions.by_version.end() && have.count(exact->second)) {
        return {Resolution::Source::ExactVersion, exact->second};
    }

    // 2. Fallback: highest version whose mapped build_id has cheats.
    //    (by_version is a std::map ordered ascending by version; iterate descending.)
    for (auto it = versions.by_version.rbegin(); it != versions.by_version.rend(); ++it) {
        if (have.count(it->second)) {
            return {Resolution::Source::FallbackOlderBuild, it->second};
        }
    }

    // 3. Nothing mapped has cheats.
    return {Resolution::Source::NotInDb, ""};
}

} // namespace thomaz::core
