#pragma once
#include <curl/curl.h>
#include <cstdio>
#include "platform/tls_policy.hpp"

namespace thomaz {

// Process-global latch: set to true the first time apply_curl_tls detects a
// missing CA bundle (ca_ok == false on Switch). One-way flag, never cleared.
// The SEC-03 UI banner reads this via tls_is_insecure() to warn the user.
// Lives OUTSIDE #ifdef __SWITCH__ so the host build can inspect it (always false
// on desktop because the desktop branch never sets it).
inline bool& tls_insecure_flag() {
    static bool f = false;
    return f;
}

inline bool tls_is_insecure() {
    return tls_insecure_flag();
}

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
        auto p = tls_policy(true);
        curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, p.verifypeer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, p.verifyhost);
    } else {
        // Fail-safe: keep the app usable rather than bricking all HTTPS.
        // Set the insecure latch so the SEC-03 banner can warn the user.
        auto p = tls_policy(false);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, p.verifypeer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, p.verifyhost);
        tls_insecure_flag() = true;
    }
#else
    auto p = tls_policy(true);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, p.verifypeer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, p.verifyhost);
#endif
}

} // namespace thomaz
