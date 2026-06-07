#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace thomaz {

struct HttpResponse {
    long status = 0;       // HTTP status code (0 = transport/connection failure)
    std::string body;      // response body (the JSON document)
    std::vector<std::pair<std::string, std::string>> headers;

    bool ok() const { return status >= 200 && status < 300; }
};

enum class HttpMethod { Get, Post, Put, Delete };

struct MultipartFile {
    std::string field;       // form field name (e.g. "image")
    std::string filename;    // e.g. "post.jpg"
    std::string contentType; // e.g. "image/jpeg"
    std::vector<std::uint8_t> bytes;
};

struct HttpRequest {
    HttpMethod  method = HttpMethod::Get;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body; // JSON body; ignored when multipart fields/files are set

    // Multipart/form-data (used by the cloud save upload). When either is
    // non-empty the request is sent as multipart and `body` is ignored.
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<MultipartFile> files;

    // Cooperative abort: set this to a shared_ptr<atomic<bool>> that the
    // owning activity's base destructor will flip to true.  The curl transport
    // checks it in CURLOPT_XFERINFOFUNCTION and aborts (returns 1) as soon as
    // the flag is set.  null (default) means the transfer never self-aborts
    // (happy-path / existing callers unchanged).
    std::shared_ptr<std::atomic<bool>> cancelled;

    // When false, libcurl does not auto-follow redirects (index_fetcher uses
    // this for custom-header auth so credentials are not forwarded cross-host).
    bool followRedirects = true;

    // When non-zero, abort the transfer if the body would exceed this size.
    std::size_t maxBodyBytes = 0;
};

// Non-buffering download seam (Phase 10 install engine). Chunks are forwarded to
// `sink`; the transport never accumulates the whole body in a std::string.
struct StreamRequest {
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::uint64_t rangeStart = 0;
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::function<bool(const std::uint8_t*, std::size_t)> sink;
};

struct StreamResult {
    long          status        = 0;
    bool          acceptsRanges = false;
    std::uint64_t totalSize     = 0;
    bool          ok            = false;
};

// HTTP abstraction so the UI/orchestration don't depend on libcurl.
class IHttpClient {
  public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse request(const HttpRequest& req) = 0;

    // Default stub keeps existing test doubles compiling without overrides.
    virtual StreamResult stream(const StreamRequest&) { return StreamResult{}; }

    // Convenience GET kept for existing callers (db_paths/self_update).
    HttpResponse get(const std::string& url) {
        HttpRequest r;
        r.method = HttpMethod::Get;
        r.url    = url;
        return request(r);
    }
};

} // namespace thomaz
