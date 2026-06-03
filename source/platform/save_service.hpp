#pragma once

#include <string>
#include <vector>

#include "core/backup_store.hpp" // SaveProfile, BackupEntry
#include "platform/title.hpp"    // InstalledTitle

namespace thomaz {

// Reads/writes real game save data. Switch impl uses libnx; the fake impl
// lets the full UI flow run on desktop without a console.
class ISaveService {
  public:
    virtual ~ISaveService() = default;

    // Profiles that currently have save data for this title.
    virtual std::vector<core::SaveProfile> profilesWithSave(std::uint64_t title_id) = 0;

    // Back up every profile's save for this title to the SD. On success returns
    // true; on failure returns false and sets *outError to a human message.
    virtual bool backup(const InstalledTitle& title, std::string* outError) = 0;

    // Restore a backup folder back into the title's save (destructive). On
    // success returns true; on failure returns false and sets *outError.
    virtual bool restore(const core::BackupEntry& entry, std::uint64_t title_id,
                         std::string* outError) = 0;
};

} // namespace thomaz
