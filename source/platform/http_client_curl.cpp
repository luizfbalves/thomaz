#include "platform/http_client_curl.hpp"

#include <cctype>
#include <cstring>
#include <curl/curl.h>
#include "platform/curl_tls.hpp"

namespace thomaz {

namespace {

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

struct StreamCtx {
    StreamRequest* req;
    StreamResult*  result;
};

size_t writeToSink(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<StreamCtx*>(userdata);
    if (!ctx || !ctx->req || !ctx->req->sink)
        return 0;
    const auto  len = size * nmemb;
    const auto* data = reinterpret_cast<const std::uint8_t*>(ptr);
    if (!ctx->req->sink(data, len))
        return 0; // abort transfer
    return len;
}

bool headerLineHasAcceptRanges(const char* line, std::size_t len) {
    constexpr const char* kNeedle = "Accept-Ranges:";
    const std::size_t     kLen    = std::strlen(kNeedle);
    if (len < kLen)
        return false;
    for (std::size_t i = 0; i < kLen; ++i) {
        if (std::tolower(static_cast<unsigned char>(line[i])) !=
            static_cast<unsigned char>(kNeedle[i]))
            return false;
    }
    const char* p = line + kLen;
    while (p < line + len && (*p == ' ' || *p == '\t'))
        ++p;
    return (p + 5 <= line + len) &&
           std::tolower(static_cast<unsigned char>(p[0])) == 'b' &&
           std::tolower(static_cast<unsigned char>(p[1])) == 'y' &&
           std::tolower(static_cast<unsigned char>(p[2])) == 't' &&
           std::tolower(static_cast<unsigned char>(p[3])) == 'e' &&
           std::tolower(static_cast<unsigned char>(p[4])) == 's';
}

size_t streamHeaderCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* ctx = static_cast<StreamCtx*>(userdata);
    if (!ctx || !ctx->result)
        return size * nmemb;
    const std::size_t len = size * nmemb;
    if (headerLineHasAcceptRanges(ptr, len))
        ctx->result->acceptsRanges = true;
    return len;
}

} // namespace

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

// CURLSH lock/unlock callbacks. userptr is the client's mutex array; `data`
// indexes which shared resource (connection cache, TLS sessions, DNS) is locked.
using ShareLocks = std::array<std::mutex, 8>;
void shareLockCb(CURL*, curl_lock_data data, curl_lock_access, void* userptr) {
    (*static_cast<ShareLocks*>(userptr))[data].lock();
}
void shareUnlockCb(CURL*, curl_lock_data data, void* userptr) {
    (*static_cast<ShareLocks*>(userptr))[data].unlock();
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

    // Share the connection cache, TLS sessions and DNS cache across every easy
    // handle so repeated image fetches reuse the live connection (keep-alive)
    // instead of doing a fresh TCP+TLS handshake each time — the big win for
    // grids of thumbnails fetched in series on Borealis' single async thread.
    share = curl_share_init();
    if (share) {
        curl_share_setopt(share, CURLSHOPT_LOCKFUNC, shareLockCb);
        curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, shareUnlockCb);
        curl_share_setopt(share, CURLSHOPT_USERDATA, &shareLocks);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
        curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    }
}

CurlHttpClient::~CurlHttpClient() {
    if (share) curl_share_cleanup(share);
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
    // Reuse pooled connections / TLS sessions / DNS across requests (see ctor).
    if (share) curl_easy_setopt(curl, CURLOPT_SHARE, share);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

    // Cooperative abort: ONLY install the progress hook when the caller actually
    // supplied a cancelled flag (WR-02). The vast majority of requests pass no
    // flag; installing the hook for them would add a per-tick indirect call +
    // shared_ptr deref to all HTTP traffic for no benefit. The CancelCtx is
    // stack-allocated and outlasts the easy_perform call below, so the pointer
    // remains valid throughout.
    CancelCtx cancelCtx{req.cancelled}; // shared_ptr copy by value
    if (req.cancelled) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlCancelXferInfo);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelCtx);
    }

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
    else if (rc == CURLE_ABORTED_BY_CALLBACK)
        response.body.clear(); // cooperative teardown abort — discard any partial body, status stays 0
    else
        response.status = 0; // transport failure

    if (mime) curl_mime_free(mime);
    if (headerList) curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return response;
}

StreamResult CurlHttpClient::stream(const StreamRequest& req) {
    StreamResult result;
    if (!networkReady || !req.sink)
        return result;

    CURL* curl = curl_easy_init();
    if (!curl)
        return result;

    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    if (share)
        curl_easy_setopt(curl, CURLOPT_SHARE, share);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    StreamCtx streamCtx{const_cast<StreamRequest*>(&req), &result};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToSink);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &streamCtx);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, streamHeaderCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &streamCtx);

    if (req.rangeStart != 0)
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE,
                         static_cast<curl_off_t>(req.rangeStart));

    CancelCtx cancelCtx{req.cancelled};
    if (req.cancelled) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlCancelXferInfo);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelCtx);
    }

    apply_curl_tls(curl);

    struct curl_slist* headerList = nullptr;
    for (const auto& h : req.headers) {
        std::string line = h.first + ": " + h.second;
        headerList       = curl_slist_append(headerList, line.c_str());
    }
    if (headerList)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);

    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
        curl_off_t contentLen = 0;
        if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLen) ==
                CURLE_OK &&
            contentLen > 0)
            result.totalSize = static_cast<std::uint64_t>(contentLen);
        result.ok = (result.status >= 200 && result.status < 300);
    } else if (rc == CURLE_WRITE_ERROR) {
        // sink returned false (cap/abort) — still report status if available
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
    } else if (rc != CURLE_ABORTED_BY_CALLBACK)
        result.status = 0;

    if (headerList)
        curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return result;
}

} // namespace thomaz
