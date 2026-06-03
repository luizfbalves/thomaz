#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/feed/feed_types.hpp"

namespace thomaz {

// AuthResult/ActionResult now live in core/feed/feed_types.hpp (so pure core
// code can produce them). Re-expose them unqualified in this namespace for the
// existing call sites (UI activities, FakeFeedClient).
using feed::ActionResult;
using feed::AuthResult;

// Contrato de rede do feed (auth + feed juntos). FakeFeedClient roda no desktop
// hoje; HttpFeedClient (futuro) pluga a API real sem tocar na UI. Todos os
// métodos são chamados de um worker thread (brls::async); não tocam na UI.
class IFeedClient {
  public:
    virtual ~IFeedClient() = default;

    // Conta
    virtual AuthResult registerUser(const std::string& user, const std::string& pass) = 0;
    virtual AuthResult login(const std::string& user, const std::string& pass) = 0;

    // Feed (cursor vazio = primeira página)
    virtual feed::FeedPage fetchFeed(const std::string& cursor) = 0;

    // Postar (bytes JPEG vindos do IAlbumSource + jogo já resolvido)
    virtual ActionResult createPost(const std::string& token,
                                    const std::vector<std::uint8_t>& jpeg,
                                    const std::string& caption,
                                    std::uint64_t gameTitleId,
                                    const std::string& gameName) = 0;

    // Curtir / descurtir
    virtual ActionResult setLike(const std::string& token,
                                 const std::string& postId, bool liked) = 0;

    // Comentários
    virtual std::vector<feed::Comment> fetchComments(const std::string& postId) = 0;
    virtual ActionResult addComment(const std::string& token,
                                    const std::string& postId, const std::string& text) = 0;
};

} // namespace thomaz
