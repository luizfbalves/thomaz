#include "doctest.h"
#include "platform/tls_policy.hpp"

using thomaz::tls_policy;

// TEST-03: regression guard for the TLS fail-safe branch.
//
// tls_policy() is the D-06 seam: the verification decision is a pure,
// host-compilable function. These tests assert that:
//   (a) the fail-safe {0,0} is intentional — a code change that accidentally
//       disables verification will fail CI here.
//   (b) the secure {1,2} is correct — verifypeer=1, verifyhost=2.
//
// The test TU intentionally includes only the curl-free tls_policy.hpp header
// (not curl_tls.hpp) so it never pulls in the curl headers and can run on the
// host build without the curl library linked.

TEST_CASE("fail-safe: ca_present==false returns no-verification policy") {
    auto p = tls_policy(false);
    CHECK(p.verifypeer == 0L);
    CHECK(p.verifyhost == 0L);
}

TEST_CASE("secure: ca_present==true returns full-verification policy") {
    auto p = tls_policy(true);
    CHECK(p.verifypeer == 1L);
    CHECK(p.verifyhost == 2L);
}
