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
#include "platform/auth_client.hpp"

namespace thomaz {

// Screen 2: shows one game's last-backup date, a "back up now" button, and the
// list of existing backups, each restorable.
class SaveDetailActivity : public brls::Activity
{
  public:
    SaveDetailActivity(InstalledTitle title, ISaveService* saveService,
                       ICloudSaveClient* cloudSaves, IAuthClient* feed);
    ~SaveDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/save_detail.xml");

    void onContentAvailable() override;

  private:
    void refreshHistory();
    void doBackup();
    void doRestore(const core::BackupEntry& entry);
    void performRestore(const core::BackupEntry& entry);
    void doDelete(const core::BackupEntry& entry);   // confirm, then remove the backup

    void doUpload();        // cloud upload flow (see pushAtRevision)
    void pushAtRevision(int revision);
    void doDownload();      // cloud download flow -> local backup + restore prompt
    void refreshCloudStatus();
    void showCloudLoggedOut();
    void promptLogin();     // always pushes the auth screen (re-login)
    bool requireSession();
    void setCloudStatusText(const std::string& text);
    std::string cloudErrorText(const std::string& apiError) const;

    // Threading contract (CONC-01): cloudBusy is read and written exclusively on the main thread
    // today (all callers — doUpload, pushAtRevision, doDownload — run their guard checks and set
    // operations inside brls::sync or direct UI callbacks, which execute on the main thread).
    // std::atomic enforces a well-defined memory model so that a future off-thread refactor
    // (Phase 4 runAsync) cannot silently introduce a data race.  Use .load() to read and
    // .store() to write at every site; do NOT use compare_exchange — the existing
    // check-then-set semantics must be preserved verbatim.
    std::atomic<bool> cloudBusy{false}; // guards against concurrent upload/download

    InstalledTitle title;
    ISaveService* saveService;
    ICloudSaveClient* cloudSaves;
    IAuthClient* feed;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace thomaz
