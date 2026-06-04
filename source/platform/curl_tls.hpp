#pragma once
#include <curl/curl.h>
#include <cstdio>

namespace thomaz {

// Enable TLS certificate verification on a curl easy handle.
//   - Switch: verify against the CA bundle packed into romfs
//     (resources/cacert.pem -> romfs:/cacert.pem). FAIL-SAFE: if that bundle
//     can't be opened (a packaging/mount bug), degrade to NO verification
//     instead of breaking ALL networking — including the self-updater that
//     would be needed to ship a fix. The bundle is read-only in romfs, so this
//     only triggers on our own build error, not an attacker-removable file.
//   - Desktop: verify against the system CA store.
// Call once per handle, replacing any SSL_VERIFYPEER/VERIFYHOST the caller set.
inline void apply_curl_tls(CURL* curl) {
#ifdef __SWITCH__
    // Probe the bundle once. curl's mbedtls backend loads CAINFO via the same
    // newlib fopen, so a successful probe means CAINFO will resolve too.
    static const bool ca_ok = [] {
        if (std::FILE* f = std::fopen("romfs:/cacert.pem", "rb")) {
            std::fclose(f);
            return true;
        }
        return false;
    }();
    if (ca_ok) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        // Fail-safe: keep the app usable rather than bricking all HTTPS.
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
#else
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#endif
}

} // namespace thomaz
