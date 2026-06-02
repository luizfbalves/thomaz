#include "doctest.h"
#include "core/cheat_txt.hpp"

using thomaz::core::parse_txt;
using thomaz::core::Cheat;

TEST_CASE("parse_txt splits regular and master cheats") {
    // Real shape from Super Mario Odyssey (spike example).
    const std::string body =
        "{Master Code}\n"
        "580F0000 0149D940\n"
        "\n"
        "[Infinite Health Save 1]\n"
        "11160000 5C3BE7DC 00000000\n"
        "01100000 5C3BE7DC 00000006\n"
        "20000000\n"
        "\n"
        "[9999 Coins Save 1]\n"
        "02100000 5C27B318 0000270f\n";

    auto cheats = parse_txt(body);
    REQUIRE(cheats.size() == 3);

    CHECK(cheats[0].is_master == true);
    CHECK(cheats[0].name == "Master Code");
    CHECK(cheats[0].opcode_lines == std::vector<std::string>{"580F0000 0149D940"});

    CHECK(cheats[1].is_master == false);
    CHECK(cheats[1].name == "Infinite Health Save 1");
    CHECK(cheats[1].opcode_lines.size() == 3);
    CHECK(cheats[1].opcode_lines[2] == "20000000");

    CHECK(cheats[2].name == "9999 Coins Save 1");
    CHECK(cheats[2].opcode_lines == std::vector<std::string>{"02100000 5C27B318 0000270f"});
}

TEST_CASE("parse_txt ignores text before the first header and trailing whitespace") {
    const std::string body =
        "some attribution line\n"
        "[Only Cheat]\r\n"      // tolerate CRLF
        "  04000000 0000 \n";   // leading/trailing spaces trimmed
    auto cheats = parse_txt(body);
    REQUIRE(cheats.size() == 1);
    CHECK(cheats[0].name == "Only Cheat");
    CHECK(cheats[0].opcode_lines == std::vector<std::string>{"04000000 0000"});
}

TEST_CASE("parse_txt on empty input returns no cheats") {
    CHECK(parse_txt("").empty());
    CHECK(parse_txt("\n\n  \n").empty());
}

#include "core/cheat_txt.hpp"  // already included above; harmless

using thomaz::core::serialize_txt;

TEST_CASE("serialize_txt always includes master and only enabled regulars") {
    std::vector<Cheat> cheats = {
        {"Master", true,  {"580F0000 0149D940"}},
        {"Infinite Health", false, {"01100000 5C3BE7DC 00000006", "20000000"}},
        {"9999 Coins", false, {"02100000 5C27B318 0000270f"}},
    };
    // Only "9999 Coins" enabled; master comes through regardless; "Infinite Health" excluded.
    std::string out = serialize_txt(cheats, {"9999 Coins"});

    const std::string expected =
        "{Master}\n"
        "580F0000 0149D940\n"
        "\n"
        "[9999 Coins]\n"
        "02100000 5C27B318 0000270f\n"
        "\n";
    CHECK(out == expected);
}

TEST_CASE("serialize_txt round-trips through parse_txt") {
    std::vector<Cheat> cheats = {
        {"M", true, {"AAAA"}},
        {"A", false, {"1111", "2222"}},
    };
    std::string out = serialize_txt(cheats, {"A"});
    auto reparsed = parse_txt(out);
    REQUIRE(reparsed.size() == 2);
    CHECK(reparsed[0].is_master == true);
    CHECK(reparsed[0].name == "M");
    CHECK(reparsed[1].name == "A");
    CHECK(reparsed[1].opcode_lines == std::vector<std::string>{"1111", "2222"});
}

TEST_CASE("serialize_txt with nothing enabled still emits the master") {
    std::vector<Cheat> cheats = {
        {"Master", true, {"AAAA"}},
        {"X", false, {"1111"}},
    };
    std::string out = serialize_txt(cheats, {});
    CHECK(out == "{Master}\nAAAA\n\n");
}
