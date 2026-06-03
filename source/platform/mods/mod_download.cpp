#include "platform/mods/mod_download.hpp"

#include <curl/curl.h>
#include <cstdio>
#include <sys/stat.h>

namespace thomaz {

namespace {

void ensure_parent_dirs(const std::string& path) {
    for (std::size_t i = 1; i < path.size(); ++i)
        if (path[i] == '/')
            ::mkdir(path.substr(0, i).c_str(), 0777); // ignore EEXIST
}

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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode rc = curl_easy_perform(curl);
    long httpStatus = 0;
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    curl_easy_cleanup(curl);

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
