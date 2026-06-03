#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <borealis.hpp>
#include "platform/feed/feed_client.hpp"
#include "platform/feed/album_source.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Feed master-detail: lista de posts (esq) + painel de detalhe/comentários (dir).
// Scroll infinito por cursor. Curtir/comentar exigem sessão (senão abre Auth).
class FeedActivity : public brls::Activity {
  public:
    FeedActivity(IFeedClient* client, IAlbumSource* album, ITitleService* titles);
    ~FeedActivity() override;

    CONTENT_FROM_XML_RES("activity/feed.xml");
    void onContentAvailable() override;

  private:
    void loadFirstPage();
    void loadNextPage();
    void renderNewRows(size_t fromIndex);
    void showDetail(const std::string& postId);
    void onComposePressed();
    bool requireSession();

    IFeedClient*   client;
    IAlbumSource*  album;
    ITitleService* titles;

    std::vector<feed::Post> posts;
    std::string nextCursor;
    bool hasMore = false;
    bool loading = false;
    std::string selectedId;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
