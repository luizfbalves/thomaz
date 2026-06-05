#include "doctest.h"
#include "core/storage_format.hpp"

using namespace thomaz;

TEST_CASE("storage_format: format_bytes") {
    CHECK(format_bytes(0)              == "0 B");
    CHECK(format_bytes(999)            == "999 B");
    CHECK(format_bytes(1000)           == "1.0 KB");
    CHECK(format_bytes(1500)           == "1.5 KB");
    CHECK(format_bytes(1'000'000)      == "1.0 MB");
    CHECK(format_bytes(512'000'000)    == "512.0 MB");
    CHECK(format_bytes(1'000'000'000)  == "1.0 GB");
    CHECK(format_bytes(48'318'382'080) == "48.3 GB");
}

TEST_CASE("storage_format: used_ratio") {
    CHECK(used_ratio(0, 0)    == 0.0f);    // total==0 guard
    CHECK(used_ratio(100, 100) == 0.0f);   // free==total → nothing used
    CHECK(used_ratio(100, 0)   == 1.0f);   // free==0 → full
    CHECK(used_ratio(100, 40)  == doctest::Approx(0.60f));  // 60 used / 100 total
    CHECK(used_ratio(100, 110) == 1.0f);   // free>total → treat as completely used
}
