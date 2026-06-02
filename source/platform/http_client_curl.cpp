#include "platform/http_client_curl.hpp"

#include <curl/curl.h>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace thomaz {

namespace {

size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

} // namespace

CurlHttpClient::CurlHttpClient() {
#ifdef __SWITCH__
    // libnx socket stack must be up before libcurl can open connections.
    networkReady = R_SUCCEEDED(socketInitializeDefault());
#else
    networkReady = true;
#endif
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CurlHttpClient::~CurlHttpClient() {
    curl_global_cleanup();
#ifdef __SWITCH__
    if (networkReady)
        socketExit();
#endif
}

HttpResponse CurlHttpClient::get(const std::string& url) {
    HttpResponse response;
    if (!networkReady)
        return response; // status 0 -> caller treats as network error

    CURL* curl = curl_easy_init();
    if (!curl)
        return response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "thomaz/0.1 (+switch homebrew)");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    // v1: skip TLS verification to avoid shipping a CA bundle. We only fetch
    // public, non-sensitive cheat text. TODO: ship cacert.pem in romfs + CAINFO.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    else
        response.status = 0; // transport failure

    curl_easy_cleanup(curl);
    return response;
}

} // namespace thomaz
