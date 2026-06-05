#include "platform/http_client_curl.hpp"

#include <curl/curl.h>
#include "platform/curl_tls.hpp"

namespace thomaz {

namespace {

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

// Per-transfer context for the abort hook.
struct CancelCtx {
    std::shared_ptr<std::atomic<bool>> cancelled; // shared_ptr copy — outlives the activity
};

// CURLOPT_XFERINFOFUNCTION: returns 1 (abort) when the cancelled flag is set,
// 0 otherwise (happy path, transfer continues normally).
int curlCancelXferInfo(void* p, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<CancelCtx*>(p);
    if (ctx && ctx->cancelled && ctx->cancelled->load()) return 1; // abort
    return 0;
}

} // namespace

CurlHttpClient::CurlHttpClient() {
#ifdef __SWITCH__
    // The libnx socket stack is already brought up by Borealis in userAppInit()
    // (lib/borealis/.../switch/switch_wrapper.c). Calling socketInitializeDefault()
    // again returns an "already initialized" error — which previously made us mark
    // the network as unavailable and silently fail EVERY request even when the
    // console was online. Sockets are guaranteed up by the time we run, so just
    // use them; teardown is likewise owned by Borealis (userAppExit -> socketExit).
    networkReady = true;
#else
    networkReady = true;
#endif
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CurlHttpClient::~CurlHttpClient() {
    curl_global_cleanup();
    // Socket teardown is owned by Borealis (userAppExit), not us — see ctor.
}

HttpResponse CurlHttpClient::request(const HttpRequest& req) {
    HttpResponse response;
    if (!networkReady)
        return response; // status 0 -> caller treats as network error

    CURL* curl = curl_easy_init();
    if (!curl)
        return response;

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    // Cooperative abort: if the caller supplied a cancelled flag, install the
    // progress hook so the transfer can be interrupted when the owning activity
    // is destroyed.  The CancelCtx is stack-allocated and outlasts the
    // easy_perform call below, so the pointer remains valid throughout.
    CancelCtx cancelCtx{req.cancelled}; // shared_ptr copy by value
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlCancelXferInfo);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelCtx);

    // TLS certificate verification via the bundled CA (romfs) / system store.
    apply_curl_tls(curl);

    switch (req.method) {
        case HttpMethod::Get:    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); break;
        case HttpMethod::Post:   curl_easy_setopt(curl, CURLOPT_POST, 1L); break;
        case HttpMethod::Put:    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT"); break;
        case HttpMethod::Delete: curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE"); break;
    }

    struct curl_slist* headerList = nullptr;
    for (const auto& h : req.headers) {
        std::string line = h.first + ": " + h.second;
        headerList = curl_slist_append(headerList, line.c_str());
    }
    if (headerList)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    curl_mime* mime = nullptr;
    const bool isMultipart = !req.files.empty() || !req.fields.empty();
    if (isMultipart) {
        mime = curl_mime_init(curl);
        for (const auto& f : req.fields) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, f.first.c_str());
            curl_mime_data(part, f.second.c_str(), CURL_ZERO_TERMINATED);
        }
        for (const auto& file : req.files) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, file.field.c_str());
            curl_mime_data(part, reinterpret_cast<const char*>(file.bytes.data()),
                           file.bytes.size());
            curl_mime_filename(part, file.filename.c_str());
            curl_mime_type(part, file.contentType.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    } else if (!req.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    else
        response.status = 0; // transport failure

    if (mime) curl_mime_free(mime);
    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return response;
}

} // namespace thomaz
