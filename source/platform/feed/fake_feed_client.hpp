#pragma once
#include <map>
#include <vector>
#include "platform/feed/feed_client.hpp"

namespace thomaz {

// Backend em memória para rodar/testar o app no desktop. Gera páginas de posts,
// aceita qualquer cadastro/login, e guarda curtidas/comentários/novos posts
// durante a sessão. Determinístico (sem rede, sem random).
class FakeFeedClient : public IFeedClient {
  public:
    FakeFeedClient();

    AuthResult registerUser(const std::string& user, const std::string& pass) override;
    AuthResult login(const std::string& user, const std::string& pass) override;
    feed::FeedPage fetchFeed(const std::string& cursor) override;
    std::vector<std::uint8_t> fetchImage(const std::string& url) override;
    ActionResult createPost(const std::string& token,
                            const std::vector<std::uint8_t>& jpeg,
                            const std::string& caption,
                            std::uint64_t gameTitleId,
                            const std::string& gameName) override;
    ActionResult setLike(const std::string& token,
                         const std::string& postId, bool liked) override;
    std::vector<feed::Comment> fetchComments(const std::string& postId) override;
    ActionResult addComment(const std::string& token,
                            const std::string& postId, const std::string& text) override;

  private:
    std::vector<feed::Post> posts; // mais novo primeiro
    std::map<std::string, std::vector<feed::Comment>> comments; // postId -> comments
    int nextId = 1000;
    std::string makeId();
};

} // namespace thomaz
