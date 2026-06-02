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
