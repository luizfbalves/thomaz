#include "doctest.h"
#include "platform/tls_policy.hpp"

using thomaz::tls_policy;
using thomaz::TlsMode;

// TEST-03: regression guard for the TLS verification policy.
//
// tls_policy() is the D-06 seam: the verification decision is a pure,
// host-compilable function.
//
// CR-01 (user-authorized D-06 reversal, 2026-06-05): the CA-absent branch is now
// FAIL-CLOSED. A missing CA bundle must NOT silently disable verification — the
// default keeps verification ON ({1,2}) so a packaging defect fails loudly
// instead of downgrading every transfer to plaintext-equivalent trust. The
// fail-safe intent of this test is preserved under the new semantics: a
// regression that silently disables verification for the DEFAULT (missing-CA)
// path must fail CI here. The insecure {0,0} policy is now reachable only via an
// explicit, opt-in TlsMode::InsecureAllowed.
//
// The test TU intentionally includes only the curl-free tls_policy.hpp header
// (not curl_tls.hpp) so it never pulls in the curl headers and can run on the
// host build without the curl library linked.

TEST_CASE("fail-closed: ca_present==false defaults to FULL verification") {
    // Regression guard: the default for a missing CA bundle MUST be
    // verification-ON. If this ever flips back to {0,0}, CI fails.
    auto p = tls_policy(false);
    CHECK(p.verifypeer == 1L);
    CHECK(p.verifyhost == 2L);
}

TEST_CASE("fail-closed: ca_present==false with explicit Verify mode is full verification") {
    auto p = tls_policy(false, TlsMode::Verify);
    CHECK(p.verifypeer == 1L);
    CHECK(p.verifyhost == 2L);
}

TEST_CASE("opt-in insecure: ca_present==false + InsecureAllowed is no-verification") {
    // The insecure policy is reachable ONLY via an explicit caller opt-in.
    auto p = tls_policy(false, TlsMode::InsecureAllowed);
    CHECK(p.verifypeer == 0L);
    CHECK(p.verifyhost == 0L);
}

TEST_CASE("secure: ca_present==true returns full-verification policy") {
    auto p = tls_policy(true);
    CHECK(p.verifypeer == 1L);
    CHECK(p.verifyhost == 2L);
}
