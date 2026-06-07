#pragma once

#include <string>
#include <vector>

#include "core/games/source_link.hpp"
#include "platform/http_client.hpp"
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

struct SourceSyncResult {
    bool        ok = false;
    std::string error;
    std::string id;
};

struct SourceSyncList {
    bool                              ok = false;
    std::string                       error;
    std::vector<core::SourceConfig>   sources;
};

// Real HTTP client for owner-scoped /sources config sync (no content blobs).
// Stateless beyond baseUrl; access token passed per call. On 401 the error is
// kCloudAuthExpired (no auto-refresh). Never logs credentials or request bodies.
class HttpSourceSyncClient {
  public:
    HttpSourceSyncClient(IHttpClient* http, std::string baseUrl);

    SourceSyncList   list(const std::string& token, CancelFlag cancelled = nullptr);
    SourceSyncResult push(const std::string& token, const std::string& id,
                          const core::SourceConfig& cfg,
                          CancelFlag cancelled = nullptr);
    SourceSyncResult remove(const std::string& token, const std::string& id,
                            CancelFlag cancelled = nullptr);

  private:
    std::string sourceUrl(const std::string& id) const;

    IHttpClient* http;
    std::string  baseUrl;
};

} // namespace thomaz
