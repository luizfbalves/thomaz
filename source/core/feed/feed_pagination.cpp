#include "core/feed/feed_pagination.hpp"
#include <unordered_set>

namespace thomaz::feed {

bool merge_feed_page(std::vector<Post>& acc, const FeedPage& page)
{
    std::unordered_set<std::string> seen;
    seen.reserve(acc.size());
    for (const auto& p : acc)
        seen.insert(p.id);

    for (const auto& p : page.posts)
        if (seen.insert(p.id).second)
            acc.push_back(p);

    return page.hasMore;
}

Post* find_post(std::vector<Post>& acc, const std::string& id)
{
    for (auto& p : acc)
        if (p.id == id)
            return &p;
    return nullptr;
}

} // namespace thomaz::feed
