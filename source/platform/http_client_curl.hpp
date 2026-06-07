#pragma once
#include "platform/http_client.hpp"

#include <array>
#include <mutex>

namespace thomaz {

// libcurl-backed HTTP client. Works on both Switch (switch-curl portlib, needs
// libnx sockets) and desktop (system libcurl). Construct once and reuse.
//
// A CURLSH share handle is kept for the client's whole lifetime so every request
// reuses the same connection cache, TLS sessions and DNS cache. Borealis runs
// async work on a single background thread, so without this each image fetch
// would open a fresh TCP+TLS connection — 30 thumbnails = 30 TLS handshakes in
// series, which is the dominant cost on the Switch. Sharing turns that into one
// handshake plus keep-alive reuse for the rest.
class CurlHttpClient : public IHttpClient {
  public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    HttpResponse request(const HttpRequest& req) override;
    StreamResult stream(const StreamRequest& req) override;

  private:
    bool          networkReady = false; // Switch: socketInitializeDefault() succeeded
    void*         share        = nullptr; // CURLSH (curl typedefs it as void)
    // One mutex per curl_lock_data id (CURL_LOCK_DATA_LAST is 7); the share
    // handle needs lock callbacks to be safe if a request ever runs off-thread.
    std::array<std::mutex, 8> shareLocks{};
};

} // namespace thomaz
