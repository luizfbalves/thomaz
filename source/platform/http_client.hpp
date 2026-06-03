#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace thomaz {

struct HttpResponse {
    long status = 0;       // HTTP status code (0 = transport/connection failure)
    std::string body;      // response body (the JSON document)

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
};

// HTTP abstraction so the UI/orchestration don't depend on libcurl.
class IHttpClient {
  public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse request(const HttpRequest& req) = 0;

    // Convenience GET kept for existing callers (db_paths/self_update).
    HttpResponse get(const std::string& url) {
        HttpRequest r;
        r.method = HttpMethod::Get;
        r.url    = url;
        return request(r);
    }
};

} // namespace thomaz
