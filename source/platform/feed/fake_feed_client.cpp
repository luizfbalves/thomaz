#include "platform/feed/fake_feed_client.hpp"
#include <algorithm>
#include <string>

namespace thomaz {

namespace {
constexpr int kPageSize = 8;
} // namespace

FakeFeedClient::FakeFeedClient()
{
    const char* users[] = { "joao", "bea", "lucas", "mari", "kai" };
    const std::uint64_t games[] = { 0x0100000000010000ULL, 0x010000000E5EE000ULL,
                                    0x0100ABCDEF000000ULL };
    const char* names[] = { "Super Mario Odyssey", "8-BIT Demo", "Fake Quest" };
    for (int i = 0; i < 25; ++i) {
        feed::Post p;
        p.id          = "seed-" + std::to_string(i);
        p.author      = { std::string("u") + std::to_string(i % 5), users[i % 5] };
        p.imageUrl    = "fake://seed/" + std::to_string(i);
        p.caption     = "Post de exemplo #" + std::to_string(i);
        p.gameTitleId = games[i % 3];
        p.gameName    = names[i % 3];
        p.likeCount   = (i * 7) % 50;
        p.likedByMe   = false;
        p.commentCount = i % 4;
        p.createdAt   = 1780000000 - i * 3600;
        posts.push_back(p);
    }
}

std::string FakeFeedClient::makeId()
{
    return "p" + std::to_string(nextId++);
}

AuthResult FakeFeedClient::registerUser(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

AuthResult FakeFeedClient::login(const std::string& user, const std::string&)
{
    if (user.empty())
        return { false, "", "username obrigatório" };
    return { true, "fake-token-" + user, "" };
}

feed::FeedPage FakeFeedClient::fetchFeed(const std::string& cursor)
{
    size_t start = cursor.empty() ? 0 : static_cast<size_t>(std::stoul(cursor));
    feed::FeedPage page;
    size_t end = std::min(start + kPageSize, posts.size());
    for (size_t i = start; i < end; ++i)
        page.posts.push_back(posts[i]);
    page.hasMore    = end < posts.size();
    page.nextCursor = page.hasMore ? std::to_string(end) : "";
    page.ok         = true; // the fake always "succeeds"
    return page;
}

std::vector<std::uint8_t> FakeFeedClient::fetchImage(const std::string&)
{
    return {}; // o fake não tem bytes reais; a UI fica com o placeholder
}

ActionResult FakeFeedClient::createPost(const std::string&,
                                        const std::vector<std::uint8_t>&,
                                        const std::string& caption,
                                        std::uint64_t gameTitleId,
                                        const std::string& gameName)
{
    feed::Post p;
    p.id          = makeId();
    p.author      = { "me", "voce" };
    p.imageUrl    = "fake://new";
    p.caption     = caption;
    p.gameTitleId = gameTitleId;
    p.gameName    = gameName;
    p.createdAt   = 1780100000;
    posts.insert(posts.begin(), p);
    return { true, "" };
}

ActionResult FakeFeedClient::setLike(const std::string&,
                                     const std::string& postId, bool liked)
{
    for (auto& p : posts)
        if (p.id == postId) {
            if (liked && !p.likedByMe) { p.likedByMe = true; p.likeCount++; }
            else if (!liked && p.likedByMe) { p.likedByMe = false; p.likeCount--; }
            return { true, "" };
        }
    return { false, "post não encontrado" };
}

std::vector<feed::Comment> FakeFeedClient::fetchComments(const std::string& postId)
{
    auto it = comments.find(postId);
    if (it != comments.end())
        return it->second;
    return {};
}

ActionResult FakeFeedClient::addComment(const std::string&,
                                        const std::string& postId, const std::string& text)
{
    if (text.empty())
        return { false, "comentário vazio" };
    feed::Comment c;
    c.id        = makeId();
    c.author    = { "me", "voce" };
    c.text      = text;
    c.createdAt = 1780100000;
    comments[postId].push_back(c);
    for (auto& p : posts)
        if (p.id == postId) { p.commentCount++; break; }
    return { true, "" };
}

} // namespace thomaz
