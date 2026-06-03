#pragma once
#include "platform/feed/album_source.hpp"

namespace thomaz {

// Álbum falso para o desktop: gera N entradas com title IDs que o
// FakeTitleService resolve para nomes de mentira. As imagens são retângulos
// JPEG mínimos embutidos (não precisam ser bonitas — só carregar).
class FakeAlbumSource : public IAlbumSource {
  public:
    std::vector<AlbumEntry>   list() override;
    std::vector<std::uint8_t> loadFull(const std::string& id) override;
};

} // namespace thomaz
