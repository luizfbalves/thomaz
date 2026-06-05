#include "platform/mods/mod_download.hpp"
#include "platform/fs_util.hpp"

#include <curl/curl.h>
#include "platform/curl_tls.hpp"
#include <atomic>
#include <cstdio>
#include <memory>
#include <sys/stat.h>

namespace thomaz {

namespace {

// Sink that both writes to the file and tallies the bytes received, so the
// caller can compare the total against the server's advertised Content-Length
// and reject a truncated transfer (WR-06).
struct FileSink {
    std::FILE*     f      = nullptr;
    std::uint64_t  written = 0;
};

size_t writeToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* sink = static_cast<FileSink*>(userdata);
    size_t n   = std::fwrite(ptr, 1, size * nmemb, sink->f);
    sink->written += n;
    return n;
}

struct ProgressCtx {
    const std::function<void(std::uint64_t, std::uint64_t)>* cb;
    std::shared_ptr<std::atomic<bool>> cancelled; // null = never aborts (existing callers)
};

int xferInfo(void* p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<ProgressCtx*>(p);
    // Cooperative abort: if the owning activity was destroyed, bail immediately.
    if (ctx && ctx->cancelled && ctx->cancelled->load()) return 1; // abort
    if (ctx && ctx->cb && *ctx->cb) {
        // IN-01: dltotal is -1/0 while the total size is unknown. Casting -1 to
        // uint64_t yields 0xFFFF... which a progress bar renders as a nonsensical
        // denominator. Clamp to 0 and let the UI treat 0 as "indeterminate".
        std::uint64_t total = dltotal > 0 ? (std::uint64_t)dltotal : 0;
        std::uint64_t now   = dlnow > 0 ? (std::uint64_t)dlnow : 0;
        (*ctx->cb)(now, total);
    }
    return 0; // nonzero would abort
}

} // namespace

bool download_file(const std::string& url, const std::string& dest_path,
                   const std::function<void(std::uint64_t, std::uint64_t)>& progress,
                   std::string* err,
                   std::shared_ptr<std::atomic<bool>> cancelled) {
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

    ProgressCtx ctx{&progress, cancelled}; // cancelled may be null (existing callers)
    FileSink sink{out, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfo);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    // TLS certificate verification via the bundled CA (romfs) / system store.
    apply_curl_tls(curl);

    CURLcode rc = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_off_t advertisedLen = -1; // server's Content-Length for THIS transfer, -1 if unknown
    if (rc == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
        // CONTENT_LENGTH_DOWNLOAD_T reflects the final (post-redirect) response.
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &advertisedLen);
    }
    curl_easy_cleanup(curl);

    // WR-06: when the server advertised a Content-Length, require the bytes we
    // actually wrote to match it. A server that closes early after a 200 yields
    // a truncated file that otherwise passes (rc==CURLE_OK && 2xx && closeOk);
    // for non-archive downloads (e.g. the self-update .nro) there is no
    // downstream archive-EOF check to catch it.
    bool lengthOk = (advertisedLen < 0) ||
                    (sink.written == static_cast<std::uint64_t>(advertisedLen));

    // WR-02/WR-06: the lengthOk check above now rejects a short transfer when the
    // server advertised a Content-Length. A chunked / no-Content-Length response
    // (advertisedLen < 0) still cannot be length-verified here; for those, archive
    // integrity relies on the downstream extractor's EOF check
    // (libarchive_extractor.cpp surfaces r != ARCHIVE_EOF). Callers downloading
    // non-archive content over a length-less response must not assume completeness
    // from this function's success return alone.
    bool closeOk = (std::fclose(out) == 0);
    bool ok = (rc == CURLE_OK) && (httpStatus >= 200 && httpStatus < 300) && closeOk && lengthOk;
    if (!ok) {
        if (err) {
            if (rc == CURLE_ABORTED_BY_CALLBACK) { /* cooperative teardown — leave *err empty, no toast */ }
            else if (rc != CURLE_OK) *err = curl_easy_strerror(rc);
            else if (!closeOk)       *err = "write error";
            else if (!lengthOk)      *err = "truncated download (" + std::to_string(sink.written) +
                                            " of " + std::to_string(advertisedLen) + " bytes)";
            else                     *err = "HTTP " + std::to_string(httpStatus);
        }
        std::remove(dest_path.c_str());
    }
    return ok;
}

} // namespace thomaz
