#include "doctest.h"
#include "core/feed/feed_pagination.hpp"

using namespace thomaz::feed;

static Post mk(const std::string& id) { Post p; p.id = id; return p; }

TEST_CASE("merge_feed_page appends all posts into an empty accumulator") {
    std::vector<Post> acc;
    FeedPage page; page.posts = { mk("a"), mk("b") }; page.hasMore = true;
    bool more = merge_feed_page(acc, page);
    REQUIRE(acc.size() == 2);
    CHECK(acc[0].id == "a");
    CHECK(acc[1].id == "b");
    CHECK(more == true);
}

TEST_CASE("merge_feed_page skips posts whose id is already present") {
    std::vector<Post> acc = { mk("a"), mk("b") };
    FeedPage page; page.posts = { mk("b"), mk("c") }; page.hasMore = false;
    bool more = merge_feed_page(acc, page);
    REQUIRE(acc.size() == 3);
    CHECK(acc[2].id == "c");
    CHECK(more == false);
}

TEST_CASE("merge_feed_page preserves insertion order across pages") {
    std::vector<Post> acc;
    FeedPage p1; p1.posts = { mk("a"), mk("b") };
    FeedPage p2; p2.posts = { mk("c") };
    merge_feed_page(acc, p1);
    merge_feed_page(acc, p2);
    REQUIRE(acc.size() == 3);
    CHECK(acc[0].id == "a");
    CHECK(acc[2].id == "c");
}

TEST_CASE("find_post returns a pointer to the matching post or null") {
    std::vector<Post> acc = { mk("a"), mk("b") };
    Post* hit = find_post(acc, "b");
    REQUIRE(hit != nullptr);
    CHECK(hit->id == "b");
    CHECK(find_post(acc, "zzz") == nullptr);
}
