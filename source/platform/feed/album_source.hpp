#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz {

struct CaptureDate {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

struct AlbumEntry {
    std::string               id;        // chave opaca p/ loadFull
    std::uint64_t             titleId = 0; // application_id real (caps:a)
    CaptureDate               captured;
    std::vector<std::uint8_t> thumbnail; // JPEG p/ o grid
};

// Abstrai o Álbum do Switch. SwitchAlbumSource usa caps:a; FakeAlbumSource
// serve imagens de exemplo no desktop. list()/loadFull() são chamados de
// worker threads (podem ser lentos).
class IAlbumSource {
  public:
    virtual ~IAlbumSource() = default;
    virtual std::vector<AlbumEntry>   list() = 0;
    virtual std::vector<std::uint8_t> loadFull(const std::string& id) = 0;
};

} // namespace thomaz
