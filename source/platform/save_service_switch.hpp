#pragma once

#ifdef __SWITCH__

#include "platform/save_service.hpp"

namespace thomaz {

// libnx-backed save backup/restore.
class NsSaveService : public ISaveService {
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

#endif // __SWITCH__
