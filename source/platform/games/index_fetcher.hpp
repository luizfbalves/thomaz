#pragma once

#include "core/games/index_parse.hpp"
#include "core/games/source_link.hpp"
#include "platform/http_client.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace thomaz {

struct FetchedCatalog {
    thomaz::core::ParsedIndex merged;
    bool                     truncated = false;
    bool                     ok        = false;
    std::string              error;
};

FetchedCatalog fetch_index(IHttpClient*                         http,
                           const thomaz::core::SourceConfig&    cfg,
                           std::shared_ptr<std::atomic<bool>>   cancelled);

} // namespace thomaz
