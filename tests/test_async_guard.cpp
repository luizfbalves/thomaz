#include "doctest.h"
#include "core/async_guard.hpp"

#include <atomic>
#include <memory>

using namespace thomaz::core;

TEST_CASE("onSync runs when alive") {
    auto alive = std::make_shared<std::atomic<bool>>(true);
    bool ran = false;
    bool result = run_if_alive(alive, [&]{ ran = true; });
    CHECK(result);
    CHECK(ran);
}

TEST_CASE("onSync dropped when not alive (activity popped)") {
    auto alive = std::make_shared<std::atomic<bool>>(true);
    *alive = false; // simulate ~ThomazActivity dtor
    bool ran = false;
    bool result = run_if_alive(alive, [&]{ ran = true; });
    CHECK_FALSE(result);
    CHECK_FALSE(ran);
}

TEST_CASE("onSync dropped when guard is null") {
    bool ran = false;
    bool result = run_if_alive(nullptr, [&]{ ran = true; });
    CHECK_FALSE(result);
    CHECK_FALSE(ran);
}
