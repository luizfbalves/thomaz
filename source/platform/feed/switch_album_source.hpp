#pragma once
#include "platform/feed/album_source.hpp"

namespace thomaz {

// Lê o Álbum do sistema via caps:a (libnx). Só screenshots (ignora vídeos).
// init() deve ser chamado uma vez no startup (Switch); exit() no shutdown.
class SwitchAlbumSource : public IAlbumSource {
  public:
    bool init();
    void exit();
    std::vector<AlbumEntry>   list() override;
    std::vector<std::uint8_t> loadFull(const std::string& id) override;
};

} // namespace thomaz
