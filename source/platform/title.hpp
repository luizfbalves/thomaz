#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz {

// One installed application, as the UI needs to display and act on it.
struct InstalledTitle {
    std::uint64_t title_id = 0;
    std::string name;     // localized display name (from NACP)
    std::string author;   // publisher (from NACP)
    std::uint32_t version = 0; // installed application version (maps to build_id via the db)
};

// Lists installed titles. Implemented on Switch by NsTitleService; the
// interface lets the UI and tests depend on behavior, not on libnx.
class ITitleService {
  public:
    virtual ~ITitleService() = default;
    virtual std::vector<InstalledTitle> listInstalled() = 0;
};

} // namespace thomaz
