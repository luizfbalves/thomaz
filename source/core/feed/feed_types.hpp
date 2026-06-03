#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz::feed {

struct User {
    std::string id;
    std::string username;
};

struct Comment {
    std::string  id;
    User         author;
    std::string  text;
    std::int64_t createdAt = 0; // epoch seconds
};

struct Post {
    std::string   id;
    User          author;
    std::string   imageUrl;       // URL remota (real) ou chave do fake
    std::string   caption;
    std::uint64_t gameTitleId = 0; // 0 = jogo desconhecido
    std::string   gameName;        // resolvido via ITitleService no compositor
    int           likeCount   = 0;
    bool          likedByMe   = false;
    int           commentCount = 0;
    std::int64_t  createdAt   = 0;
};

struct FeedPage {
    std::vector<Post> posts;
    std::string       nextCursor; // passar de volta em fetchFeed; vazio = fim
    bool              hasMore = false;
    bool              ok      = false; // transport succeeded? (false = load failed,
                                       // distinct from a successful but empty feed)
};

struct Session {
    std::string token;        // short-lived access token
    std::string refreshToken; // long-lived, rotates on each /auth/refresh
    std::string username;
};

// Network call results (moved here from feed_client.hpp so pure core code can
// produce them without depending on the platform layer).
struct AuthResult {
    bool        ok = false;
    std::string token;        // access token when ok
    std::string error;        // human/i18n message when !ok
    std::string refreshToken; // refresh token when ok (appended last on purpose)
};

struct ActionResult {
    bool        ok = false;
    std::string error;
};

} // namespace thomaz::feed
