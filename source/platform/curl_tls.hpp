#pragma once
#include <curl/curl.h>

namespace thomaz {

// Enable TLS certificate verification on a curl easy handle. On Switch, point
// curl at the CA bundle packed into romfs (resources/cacert.pem -> romfs:/);
// on desktop, rely on the system CA store. Call once per handle, replacing any
// SSL_VERIFYPEER/VERIFYHOST=0 the caller previously set.
inline void apply_curl_tls(CURL* curl) {
#ifdef __SWITCH__
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
#endif
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
}

} // namespace thomaz
