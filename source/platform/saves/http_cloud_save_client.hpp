#pragma once
#include <string>
#include "platform/saves/cloud_save_client.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Real ICloudSaveClient backed by the thomaz-api over HTTP. Stateless beyond
// the base URL; the access token is passed per call (read from auth_store by
// the UI). On 401 the result error is kCloudAuthExpired (no auto-refresh).
class HttpCloudSaveClient : public ICloudSaveClient {
  public:
    HttpCloudSaveClient(IHttpClient* http, std::string baseUrl);

    CloudStatus getStatus(const std::string& token, std::uint64_t titleId,
                          CancelFlag cancelled = nullptr) override;
    CloudPull   pull(const std::string& token, std::uint64_t titleId,
                     CancelFlag cancelled = nullptr) override;
    CloudPush   push(const std::string& token, std::uint64_t titleId,
                     const std::vector<std::uint8_t>& blob,
                     const std::string& label, int revision,
                     CancelFlag cancelled = nullptr) override;

  private:
    std::string savesUrl(std::uint64_t titleId) const;

    IHttpClient* http;
    std::string  baseUrl; // no trailing slash
};

} // namespace thomaz
