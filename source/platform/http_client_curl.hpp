#pragma once
#include "platform/http_client.hpp"

namespace thomaz {

// libcurl-backed HTTP client. Works on both Switch (switch-curl portlib, needs
// libnx sockets) and desktop (system libcurl). Construct once and reuse.
class CurlHttpClient : public IHttpClient {
  public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    HttpResponse get(const std::string& url) override;

  private:
    bool networkReady = false; // Switch: socketInitializeDefault() succeeded
};

} // namespace thomaz
