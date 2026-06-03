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
};

struct Session {
    std::string token;
    std::string username;
};

} // namespace thomaz::feed
