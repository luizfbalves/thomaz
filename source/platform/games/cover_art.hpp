#pragma once

#include "core/games/title_id.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

enum class ArtTier { Titledb, LibnxIcon, Placeholder };

struct CoverArt {
    std::vector<std::uint8_t> bytes;
    ArtTier                     tier = ArtTier::Placeholder;
    bool                        ok   = false;
};

// Regional titledb JSON endpoint (metadata only — configurable in one place).
const char* titledb_regional_url();

// Look up iconUrl for a title id after the regional index is loaded/cached once.
std::optional<std::string> titledb_icon_url(std::uint64_t titleId);

// 3-tier cover resolution: titledb iconUrl -> installed libnx JPEG -> placeholder.
CoverArt resolve_cover(IHttpClient* http, ITitleService* titles, std::uint64_t titleId,
                       thomaz::core::TitleKind kind,
                       std::shared_ptr<std::atomic<bool>> cancelled);

} // namespace thomaz
