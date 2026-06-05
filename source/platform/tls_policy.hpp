#pragma once

namespace thomaz {

// Pure TLS verification policy — curl-free, host-compilable, Switch-guard-free.
// This is the D-06 seam: the verification decision lives here so it can be
// exercised by host doctests without any curl or platform-specific includes.
struct TlsPolicy {
    long verifypeer;  // CURLOPT_SSL_VERIFYPEER value (0=off, 1=on)
    long verifyhost;  // CURLOPT_SSL_VERIFYHOST value (0=off, 2=full)
};

// Verification mode for the CA-absent branch. The default is Verify: a missing
// CA bundle must NOT silently disable certificate validation.
enum class TlsMode { Verify, InsecureAllowed };

// Return the TLS verification policy for the given CA bundle availability.
//   ca_present == true                       → {verifypeer=1, verifyhost=2}  (full verification)
//   ca_present == false, mode==Verify        → {verifypeer=1, verifyhost=2}  (FAIL-CLOSED: the
//                                              transfer fails loudly rather than transmitting
//                                              over an unauthenticated connection)
//   ca_present == false, mode==InsecureAllowed → {verifypeer=0, verifyhost=0}  (explicit, opt-in)
//
// REVERSAL of D-06 fail-open (CR-01, user-authorized 2026-06-05): a missing CA
// bundle previously degraded to no-verification ({0,0}), which let a network
// attacker serve forged "HTTPS" content that the app then extracted/installed.
// The default is now fail-closed: verification stays ON unless a caller
// explicitly opts into InsecureAllowed for a narrowly-scoped channel. The bundle
// is read-only in romfs, so a missing bundle is a packaging defect — the correct
// response is a loud transfer failure, not silent downgrade to plaintext trust.
inline TlsPolicy tls_policy(bool ca_present, TlsMode mode = TlsMode::Verify) {
    if (ca_present)
        return TlsPolicy{1L, 2L};
    return (mode == TlsMode::InsecureAllowed) ? TlsPolicy{0L, 0L} : TlsPolicy{1L, 2L};
}

} // namespace thomaz
