#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <borealis.hpp>
#include "platform/feed/feed_client.hpp"
#include "platform/feed/album_source.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Compositor split: grid do Álbum (esq) + preview/legenda/postar (dir).
// Ao selecionar uma captura, resolve o jogo (titleId -> nome) via ITitleService.
class ComposerActivity : public brls::Activity {
  public:
    ComposerActivity(IFeedClient* client, IAlbumSource* album, ITitleService* titles,
                     std::function<void()> onPosted);
    ~ComposerActivity() override;

    CONTENT_FROM_XML_RES("activity/composer.xml");
    void onContentAvailable() override;

  private:
    void loadAlbum();
    void selectEntry(const AlbumEntry& entry);
    void doPost();
    std::string resolveGameName(std::uint64_t titleId);

    IFeedClient*   client;
    IAlbumSource*  album;
    ITitleService* titles;
    std::function<void()> onPosted;

    std::string selectedEntryId;
    std::uint64_t selectedTitleId = 0;
    std::string selectedGameName;
    std::vector<std::uint8_t> selectedFullBytes; // cached by selectEntry, reused by doPost
    std::string caption;
    bool busy = false;

    std::shared_ptr<std::atomic_bool> alive = std::make_shared<std::atomic_bool>(true);
};

} // namespace thomaz
