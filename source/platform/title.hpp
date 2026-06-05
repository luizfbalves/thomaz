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
    std::string display_version; // human version string from NACP (e.g. "1.0.1"); may be empty
    std::vector<std::uint8_t> icon; // raw JPEG icon bytes from control data (empty if none)
    std::uint64_t save_data_size = 0; // configured user-account save-data size in bytes (NACP); 0 = unknown
    std::uint8_t startup_user_account = 0; // NACP startup_user_account; 0 = no account required (homebrew/forwarder heuristic)
};

// Lists installed titles. Implemented on Switch by NsTitleService; the
// interface lets the UI and tests depend on behavior, not on libnx.
class ITitleService {
  public:
    virtual ~ITitleService() = default;
    virtual std::vector<InstalledTitle> listInstalled() = 0;
};

} // namespace thomaz
