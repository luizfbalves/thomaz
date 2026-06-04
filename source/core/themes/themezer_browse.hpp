#pragma once
#include "core/themes/themezer_types.hpp"
#include <functional>
#include <optional>
#include <string>

// NOTE: BrowseStatus/BrowseResult also exist in core/mods/mod_browse.hpp with a
// different layout (SearchPage vs BrowsePage). Defining them again in the same
// thomaz::core namespace is an ODR violation that corrupts memory at link time.
// The themezer browse API therefore lives in the nested thomaz::core::themezer
// namespace to stay collision-free.
namespace thomaz::core::themezer {

// Performs the GraphQL POST: takes the request body, returns the response body,
// or nullopt on transport failure. Injected so the core stays testable.
using GraphQlFetcher = std::function<std::optional<std::string>(const std::string& body)>;

enum class BrowseStatus { Ok, NetworkError };
struct BrowseResult {
    BrowseStatus status = BrowseStatus::NetworkError;
    BrowsePage   page;
};

BrowseResult browse_themes(const std::string& query, const std::string& target,
                           int page, int limit, const GraphQlFetcher& fetch);
BrowseResult browse_packs(const std::string& query, int page, int limit,
                          const GraphQlFetcher& fetch);

enum class DetailStatus { Ok, NotFound, NetworkError };
struct DetailResult {
    DetailStatus status = DetailStatus::NetworkError;
    ThemeDetail  detail;
};

DetailResult theme_detail(const std::string& hex_id, const GraphQlFetcher& fetch);
DetailResult pack_detail(const std::string& hex_id, const GraphQlFetcher& fetch);

} // namespace thomaz::core::themezer
