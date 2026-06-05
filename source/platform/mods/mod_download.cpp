#include "platform/mods/mod_download.hpp"
#include "platform/fs_util.hpp"

#include <curl/curl.h>
#include "platform/curl_tls.hpp"
#include <cstdio>
#include <sys/stat.h>

namespace thomaz {

namespace {

size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* f = static_cast<std::FILE*>(userdata);
    return std::fwrite(ptr, 1, size * nmemb, f);
}

struct ProgressCtx {
    const std::function<void(std::uint64_t, std::uint64_t)>* cb;
};

int xferInfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<ProgressCtx*>(p);
    if (ctx && ctx->cb && *ctx->cb)
        (*ctx->cb)((std::uint64_t)dlnow, (std::uint64_t)dltotal);
    return 0; // nonzero would abort
}

} // namespace

bool download_file(const std::string& url, const std::string& dest_path,
                   const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                   std::string* err) {
    ensure_parent_dirs(dest_path);

    std::FILE* out = std::fopen(dest_path.c_str(), "wb");
    if (!out) {
        if (err) *err = "cannot open " + dest_path;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::fclose(out);
        if (err) *err = "curl init failed";
        return false;
    }

    ProgressCtx ctx{&progress};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfo);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    // TLS certificate verification via the bundled CA (romfs) / system store.
    apply_curl_tls(curl);

    CURLcode rc = curl_easy_perform(curl);
    long httpStatus = 0;
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);

    // WR-02: download_file does NOT by itself guarantee a byte-complete file.
    // A server that closes early after a 200 (or a chunked/no-Content-Length
    // response) can yield a truncated archive that still passes
    // (rc==CURLE_OK && 2xx && closeOk). Integrity of downloaded archives
    // therefore depends on the downstream extractor's EOF check
    // (libarchive_extractor.cpp surfaces r != ARCHIVE_EOF), which rejects a
    // truncated archive. Callers that download non-archive content must not
    // assume completeness from this function's success return alone.
    bool closeOk = (std::fclose(out) == 0);
    bool ok = (rc == CURLE_OK) && (httpStatus >= 200 && httpStatus < 300) && closeOk;
    if (!ok) {
        if (err) {
            if (rc != CURLE_OK) *err = curl_easy_strerror(rc);
            else if (!closeOk)  *err = "write error";
            else                *err = "HTTP " + std::to_string(httpStatus);
        }
        std::remove(dest_path.c_str());
    }
    return ok;
}

} // namespace thomaz
