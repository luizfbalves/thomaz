#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "core/backup_store.hpp"
#include "platform/save_service.hpp"
#include "platform/title.hpp"
#include "platform/saves/cloud_save_client.hpp"
#include "platform/feed/feed_client.hpp"

namespace thomaz {

// Screen 2: shows one game's last-backup date, a "back up now" button, and the
// list of existing backups, each restorable.
class SaveDetailActivity : public brls::Activity
{
  public:
    SaveDetailActivity(InstalledTitle title, ISaveService* saveService,
                       ICloudSaveClient* cloudSaves, IFeedClient* feed);
    ~SaveDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/save_detail.xml");

    void onContentAvailable() override;

  private:
    void refreshHistory();
    void doBackup();
    void doRestore(const core::BackupEntry& entry);
    void performRestore(const core::BackupEntry& entry);

    void doUpload();        // cloud upload flow (see pushAtRevision)
    void pushAtRevision(int revision);
    void doDownload();      // cloud download flow -> local backup + restore prompt
    void refreshCloudStatus();
    void showCloudLoggedOut();
    bool requireSession();
    void setCloudStatusText(const std::string& text);
    std::string cloudErrorText(const std::string& apiError) const;

    bool cloudBusy     = false; // guards against concurrent upload/download (doUpload/doDownload)

    InstalledTitle title;
    ISaveService* saveService;
    ICloudSaveClient* cloudSaves;
    IFeedClient* feed;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace thomaz
