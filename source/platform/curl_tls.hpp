#pragma once
#include <curl/curl.h>
#include <atomic>
#include <cstdio>
#include "platform/tls_policy.hpp"

namespace thomaz {

// Process-global latch: set to true the first time apply_curl_tls detects a
// missing CA bundle (ca_ok == false on Switch). One-way flag, never cleared.
// The SEC-03 UI banner reads this via tls_is_insecure() to warn the user.
// Lives OUTSIDE #ifdef __SWITCH__ so the host build can inspect it (always false
// on desktop because the desktop branch never sets it).
// IN-04: atomic so the worker-thread setter (apply_curl_tls, via brls::async)
// and the UI-thread reader (install_tls_warning_banner) do not data-race. It is
// a one-way latch so the race was benign in practice, but a plain bool is still
// UB per the C++ memory model — matching the cloudBusy std::atomic migration.
inline std::atomic<bool>& tls_insecure_flag() {
    static std::atomic<bool> f{false};
    return f;
}

inline bool tls_is_insecure() {
    return tls_insecure_flag().load();
}

// Enable TLS certificate verification on a curl easy handle.
//   - Switch: verify against the CA bundle packed into romfs
//     (resources/cacert.pem -> romfs:/cacert.pem). FAIL-CLOSED (CR-01,
//     user-authorized D-06 reversal): if that bundle can't be opened (a
//     packaging/mount bug), keep verification ON so the transfer fails loudly
//     rather than silently downloading/installing attacker-forged content over
//     an unauthenticated connection. A missing read-only romfs bundle is a build
//     defect; the correct response is a visible failure, not a silent downgrade.
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
        // FAIL-CLOSED: verification stays ON (tls_policy(false) defaults to
        // TlsMode::Verify → {1,2}). The transfer will fail loudly. We do NOT set
        // the insecure latch here: the insecure path is now an explicit, opt-in
        // caller decision (TlsMode::InsecureAllowed) and no content-bearing
        // download requests it. The SEC-03 banner therefore only renders if some
        // future narrowly-scoped channel deliberately opts into the insecure mode
        // and sets the latch itself.
        auto p = tls_policy(false);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, p.verifypeer);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, p.verifyhost);
    }
#else
    auto p = tls_policy(true);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, p.verifypeer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, p.verifyhost);
#endif
}

} // namespace thomaz
