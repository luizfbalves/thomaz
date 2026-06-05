#include "doctest.h"
#include "platform/title.hpp"
#include "core/title_filter.hpp"

using namespace thomaz;
using namespace thomaz::core;

// Helpers to build InstalledTitle inline without any service dependency.
static InstalledTitle make_title(std::uint64_t id, std::uint64_t saveSize, std::uint8_t acct)
{
    InstalledTitle t;
    t.title_id             = id;
    t.name                 = "Test";
    t.save_data_size       = saveSize;
    t.startup_user_account = acct;
    return t;
}

// ------------------------------------------------------------------
// classify()
// ------------------------------------------------------------------

TEST_CASE("title_filter: classify Game — save > 0, acct > 0 (SMO-like)")
{
    auto smo = make_title(0x0100000000010000ULL, 32ULL * 1024 * 1024, 1);
    CHECK(classify(smo) == TitleKind::Game);
}

TEST_CASE("title_filter: classify NonGame — save == 0 and acct == 0 (Sphaira-like)")
{
    auto sphaira = make_title(0x0500000000000001ULL, 0, 0);
    CHECK(classify(sphaira) == TitleKind::NonGame);
}

TEST_CASE("title_filter: classify Game — save == 0 but acct > 0 (no-save real game)")
{
    // BotW has save_data_size==0 in fake data but has startup_user_account==1.
    auto botw = make_title(0x01007EF00011E000ULL, 0, 1);
    CHECK(classify(botw) == TitleKind::Game);
}

TEST_CASE("title_filter: classify Game — save > 0 but acct == 0")
{
    auto t = make_title(0x0100000000000001ULL, 1024, 0);
    CHECK(classify(t) == TitleKind::Game);
}

// ------------------------------------------------------------------
// effectively_hidden()
// ------------------------------------------------------------------

TEST_CASE("title_filter: effectively_hidden false for real game without overrides")
{
    auto smo = make_title(0x0100000000010000ULL, 32ULL * 1024 * 1024, 1);
    CHECK_FALSE(effectively_hidden(smo, {}, {}));
}

TEST_CASE("title_filter: effectively_hidden true for NonGame without overrides")
{
    auto sphaira = make_title(0x0500000000000001ULL, 0, 0);
    CHECK(effectively_hidden(sphaira, {}, {}));
}

TEST_CASE("title_filter: force_shown overrides NonGame heuristic → visible")
{
    auto botw = make_title(0x01007EF00011E000ULL, 0, 0); // would be NonGame without override
    std::set<std::uint64_t> force_shown = { botw.title_id };
    CHECK_FALSE(effectively_hidden(botw, {}, force_shown));
}

TEST_CASE("title_filter: force_hidden overrides Game heuristic → hidden")
{
    auto smo = make_title(0x0100000000010000ULL, 32ULL * 1024 * 1024, 1);
    std::set<std::uint64_t> force_hidden = { smo.title_id };
    CHECK(effectively_hidden(smo, force_hidden, {}));
}

TEST_CASE("title_filter: force_shown takes priority over force_hidden for same id")
{
    auto t = make_title(0xDEADBEEF00000001ULL, 0, 0);
    std::set<std::uint64_t> force_hidden = { t.title_id };
    std::set<std::uint64_t> force_shown  = { t.title_id };
    // force_shown wins (checked first).
    CHECK_FALSE(effectively_hidden(t, force_hidden, force_shown));
}
