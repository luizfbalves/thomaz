#pragma once

namespace thomaz {

// Pure TLS verification policy — curl-free, host-compilable, Switch-guard-free.
// This is the D-06 seam: the verification decision lives here so it can be
// exercised by host doctests without any curl or platform-specific includes.
struct TlsPolicy {
    long verifypeer;  // CURLOPT_SSL_VERIFYPEER value (0=off, 1=on)
    long verifyhost;  // CURLOPT_SSL_VERIFYHOST value (0=off, 2=full)
};

// Return the TLS verification policy for the given CA bundle availability.
//   ca_present == true  → {verifypeer=1, verifyhost=2}  (full certificate verification)
//   ca_present == false → {verifypeer=0, verifyhost=0}  (fail-safe: no verification)
//
// LOCKED behavior (D-03/SEC-03): when the CA bundle is absent the app degrades to
// no-verification rather than breaking all HTTPS networking (keeps the self-updater
// alive on a packaging defect). The bundle is read-only in romfs, so this only
// triggers on our own build error, not an attacker-removable file.
inline TlsPolicy tls_policy(bool ca_present) {
    return ca_present ? TlsPolicy{1L, 2L} : TlsPolicy{0L, 0L};
}

} // namespace thomaz
