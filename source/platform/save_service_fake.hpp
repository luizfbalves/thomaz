#pragma once

#ifndef __SWITCH__

#include "platform/save_service.hpp"

namespace thomaz {

// Desktop stand-in: writes dummy backup folders under saves_root() so the UI
// flow (list, last-backup date, history, restore) is exercisable without a console.
class FakeSaveService : public ISaveService {
  public:
    std::vector<core::SaveProfile> profilesWithSave(std::uint64_t title_id) override;
    bool backup(const InstalledTitle& title, std::string* outError) override;
    bool restore(const core::BackupEntry& entry, std::uint64_t title_id,
                 std::string* outError) override;
    std::vector<std::uint8_t> packageActiveSave(std::uint64_t title_id,
                                                std::string* outError) override;
    bool importPackageAsBackup(std::uint64_t title_id,
                               const std::vector<std::uint8_t>& blob,
                               std::string* outError) override;
};

} // namespace thomaz

#endif // !__SWITCH__
