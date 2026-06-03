#include "doctest.h"
#include <nlohmann/json.hpp>

TEST_CASE("json library is wired in") {
    auto j = nlohmann::json::parse(R"({"a": 1})");
    CHECK(j["a"].get<int>() == 1);
}
