#include "core/cheat_repository.hpp"
#include "core/cheat_db.hpp"
#include "core/db_paths.hpp"

namespace thomaz::core {

FetchResult fetch_cheat_set(std::uint64_t title_id,
                            std::uint32_t version,
                            const UrlFetcher& fetch) {
    FetchResult result;

    // Both documents are required to resolve cheats.
    std::optional<std::string> versions_json = fetch(versions_url(title_id));
    if (!versions_json) {
        result.status = FetchStatus::NetworkError;
        return result;
    }
    std::optional<std::string> cheats_json = fetch(cheats_url(title_id));
    if (!cheats_json) {
        result.status = FetchStatus::NetworkError;
        return result;
    }

    VersionMap versions = parse_versions(*versions_json);
    std::vector<std::string> available = build_ids_with_cheats(*cheats_json);

    Resolution resolution = resolve_build_id(version, versions, available);
    if (resolution.source == Resolution::Source::NotInDb || resolution.build_id.empty()) {
        result.status = FetchStatus::NotInDb;
        return result;
    }

    result.status        = FetchStatus::Ok;
    result.set.resolution = resolution;
    result.set.cheats     = parse_db_cheats(*cheats_json, resolution.build_id);
    result.set.sd_path    = sd_cheat_path(title_id, resolution.build_id);
    result.set.title      = versions.title;
    return result;
}

} // namespace thomaz::core
