#pragma once
#include <string>

namespace thomaz {

struct HttpResponse {
    long status = 0;       // HTTP status code (0 = transport/connection failure)
    std::string body;      // response body (the JSON document)

    bool ok() const { return status == 200; }
};

// Minimal HTTP GET abstraction so the UI/orchestration don't depend on libcurl.
class IHttpClient {
  public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse get(const std::string& url) = 0;
};

} // namespace thomaz
