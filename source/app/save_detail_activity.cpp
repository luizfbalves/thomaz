#include "app/save_detail_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/game_panel.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>

#include <functional>
#include <set>

#include "core/backup_store.hpp"
#include "app/auth_activity.hpp"
#include "platform/feed/auth_store.hpp"
#include "platform/saves/sync_store.hpp"
#include "core/saves/save_sync.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {
// Render the "Last backup: {{when}}" i18n string with the timestamp substituted.
std::string lastBackupText(const std::optional<std::string>& last) {
    if (!last)
        return "thomaz/saves/never"_i18n;
    std::string s    = "thomaz/saves/last_backup"_i18n; // contains literal "{{when}}"
    std::string when = core::format_timestamp_label(*last);
    auto pos = s.find("{{when}}");
    if (pos != std::string::npos)
        s.replace(pos, 8, when);
    else
        s += " " + when;
    return s;
}
} // namespace

SaveDetailActivity::SaveDetailActivity(InstalledTitle title, ISaveService* saveService,
                                       ICloudSaveClient* cloudSaves, IAuthClient* feed)
    : title(std::move(title)), saveService(saveService), cloudSaves(cloudSaves), feed(feed)
{
}

void SaveDetailActivity::onContentAvailable()
{
    install_header_username(this);
    install_tls_warning_banner(this);
    install_help_action(this, "saveFrame", "thomaz/help/saves");

    populate_game_panel(this, this->title);

    if (auto* btn = this->getView("backupButton")) {
        btn->registerClickAction([this](brls::View*) { this->doBackup(); return true; });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }
    this->refreshHistory();

    if (auto* up = this->getView("cloudUpload")) {
        up->registerClickAction([this](brls::View*) { this->doUpload(); return true; });
        up->addGestureRecognizer(new brls::TapGestureRecognizer(up));
    }
    if (auto* down = this->getView("cloudDownload")) {
        down->registerClickAction([this](brls::View*) { this->doDownload(); return true; });
        down->addGestureRecognizer(new brls::TapGestureRecognizer(down));
    }
    if (auto* login = this->getView("cloudLogin")) {
        login->registerClickAction([this](brls::View*) { this->promptLogin(); return true; });
        login->addGestureRecognizer(new brls::TapGestureRecognizer(login));
    }

    if (load_session().has_value())
        this->refreshCloudStatus();
    else
        this->showCloudLoggedOut();
}

void SaveDetailActivity::refreshHistory()
{
    std::string root = core::saves_root();

    if (auto* lbl = dynamic_cast<brls::Label*>(this->getView("lastBackup")))
        lbl->setText(lastBackupText(core::last_backup_timestamp(root, this->title.title_id)));

    auto* box = dynamic_cast<brls::Box*>(this->getView("historyBox"));
    if (!box)
    {
        brls::Logger::error("historyBox missing or not a Box");
        return;
    }

    // If focus currently sits on a row we're about to destroy, move it to a
    // stable view first — freeing the focused view dangles Borealis' focus
    // pointer and crashes on the next focus/input event.
    if (brls::View* f = brls::Application::getCurrentFocus()) {
        for (brls::View* v = f; v; v = v->getParent()) {
            if (v == box) {
                if (auto* bb = this->getView("backupButton"))
                    brls::Application::giveFocus(bb);
                break;
            }
        }
    }

    box->clearViews();

    // A small focusable "text button" (controller A + touch) for a row action.
    auto makeActionButton = [](const std::string& text, NVGcolor color,
                               std::function<void()> onTap) -> brls::Box* {
        auto* btn = new brls::Box(brls::Axis::ROW);
        btn->setFocusable(true);
        btn->setMarginLeft(16.0f);
        btn->setPadding(6.0f, 12.0f, 6.0f, 12.0f);
        btn->setCornerRadius(8.0f);
        btn->setAlignItems(brls::AlignItems::CENTER);
        btn->setHideHighlightBackground(true);

        auto* lbl = new brls::Label();
        lbl->setText(text);
        lbl->setFontSize(14.0f);
        lbl->setTextColor(color);
        btn->addView(lbl);

        btn->registerClickAction([onTap](brls::View*) { onTap(); return true; });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
        return btn;
    };

    auto entries = core::list_backups(root, this->title.title_id);
    for (const auto& entry : entries) {
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(52.0f);
        row->setMarginBottom(4.0f);
        row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        row->setCornerRadius(10.0f);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));

        brls::Label* ts = new brls::Label();
        ts->setText(core::format_timestamp_label(entry.timestamp));
        ts->setFontSize(16.0f);
        ts->setGrow(1.0f);
        row->addView(ts);

        core::BackupEntry captured = entry;
        row->addView(makeActionButton("thomaz/saves/action_restore"_i18n,
                                      nvgRGB(0x92, 0x77, 0xFF),
                                      [this, captured]() { this->doRestore(captured); }));
        row->addView(makeActionButton("thomaz/saves/action_delete"_i18n,
                                      nvgRGB(0xE0, 0x57, 0x57),
                                      [this, captured]() { this->doDelete(captured); }));
        box->addView(row);
    }
}

void SaveDetailActivity::doBackup()
{
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);

    ISaveService* svc = this->saveService;
    InstalledTitle t  = this->title;

    auto result = std::make_shared<std::pair<bool, std::string>>();
    this->runAsync(
        [svc, t, result]() {
            result->second = {};
            result->first  = svc->backup(t, &result->second);
        },
        [this, result]() {
            if (auto* spinner = this->getView("spinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            brls::Application::notify(result->first
                ? "thomaz/saves/backup_ok"_i18n
                : ("thomaz/saves/backup_fail"_i18n + std::string(": ") + result->second));
            if (result->first)
                this->refreshHistory();
        });
}

void SaveDetailActivity::performRestore(const core::BackupEntry& entry)
{
    if (!this->alive->load()) return;
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);
    ISaveService* svc   = this->saveService;
    std::uint64_t tid   = this->title.title_id;
    core::BackupEntry e = entry;

    auto result = std::make_shared<std::pair<bool, std::string>>();
    this->runAsync(
        [svc, e, tid, result]() {
            result->second = {};
            result->first  = svc->restore(e, tid, &result->second);
        },
        [this, result]() {
            if (auto* spinner = this->getView("spinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            brls::Application::notify(result->first
                ? "thomaz/saves/restore_ok"_i18n
                : ("thomaz/saves/restore_fail"_i18n + std::string(": ") + result->second));
            if (result->first)
                this->refreshHistory();
        });
}

void SaveDetailActivity::doRestore(const core::BackupEntry& entry)
{
    auto alive          = this->alive;
    core::BackupEntry e = entry;
    // Destructive — confirm first.
    brls::Dialog* dialog = new brls::Dialog("thomaz/saves/confirm_restore_body"_i18n);
    dialog->addButton("thomaz/saves/action_restore"_i18n, [this, alive, e]() {
        if (!alive->load())
            return; // activity gone before the user confirmed
        this->performRestore(e);
    });
    dialog->addButton("thomaz/common/cancel"_i18n, []() {});
    dialog->open();
}

void SaveDetailActivity::doDelete(const core::BackupEntry& entry)
{
    auto alive          = this->alive;
    core::BackupEntry e = entry;
    // Destructive + irreversible — confirm first.
    brls::Dialog* dialog = new brls::Dialog("thomaz/saves/confirm_delete_body"_i18n);
    dialog->addButton("thomaz/saves/action_delete"_i18n, [this, alive, e]() {
        if (!alive->load())
            return; // activity gone before the user confirmed
        // Defer to the next frame: the Dialog restores focus to the "Delete"
        // button (inside the row we're about to free) *after* this callback
        // returns. Tearing the row down here would dangle that focus and crash.
        // refreshHistory() moves focus to a stable view before clearing.
        brls::sync([this, alive, e]() {
            if (!alive->load())
                return;
            bool ok = core::delete_backup(e);
            brls::Application::notify(ok ? "thomaz/saves/delete_ok"_i18n
                                         : "thomaz/saves/delete_fail"_i18n);
            if (ok)
                this->refreshHistory();
        });
    });
    dialog->addButton("thomaz/common/cancel"_i18n, []() {});
    dialog->open();
}

void SaveDetailActivity::doUpload(bool autoRetry) {
    if (this->cloudBusy.load()) return;
    if (!this->requireSession()) return;
    // A user-initiated upload starts a fresh retry budget; an auto-retry from a
    // push conflict preserves the running count (WR-05).
    if (!autoRetry) this->cloudConflictRetries = 0;
    this->cloudBusy.store(true);
    this->setCloudStatusText("thomaz/saves/cloud_uploading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    std::uint64_t tid   = this->title.title_id;
    // Capture the cancellation flag by value (shared_ptr copy) BEFORE dispatch
    // so the worker never touches `this`.  Passed into each HTTP transfer so
    // destroying the activity aborts the in-flight cloud-save request (CONC-03).
    auto cancelled = this->cancelledFlag();

    auto status = std::make_shared<CloudStatus>();
    this->runAsync(
        [c, token, tid, status, cancelled]() {
            // Fresh status decides clean push vs conflict.
            *status = c->getStatus(token, tid, cancelled);
        },
        [this, status, tid]() {
            if (!status->ok) {
                this->cloudBusy.store(false);
                if (status->error == kCloudAuthExpired) this->showCloudLoggedOut();
                this->setCloudStatusText(this->cloudErrorText(status->error));
                return;
            }
            int synced = load_synced_revision(tid);
            core::SyncSituation sit = core::classify(status->exists, status->revision, synced);
            core::PushPlan plan = core::plan_push(sit, status->revision);
            if (plan.isConflict) {
                this->cloudBusy.store(false); // wait on the user's choice
                int rev = plan.revision;
                brls::Dialog* dlg = new brls::Dialog("thomaz/saves/cloud_conflict_body"_i18n);
                dlg->addButton("thomaz/saves/cloud_send_mine"_i18n, [this, alive = this->alive, rev]() {
                    if (!alive->load()) return;
                    this->pushAtRevision(rev);
                });
                dlg->addButton("thomaz/saves/cloud_keep_cloud"_i18n, [this, alive = this->alive]() {
                    if (!alive->load()) return;
                    this->doDownload();
                });
                dlg->open();
                return;
            }
            // Clean push (NoCloud -> rev 0, InSync -> current rev).
            this->pushAtRevision(plan.revision);
        });
}

void SaveDetailActivity::pushAtRevision(int revision) {
    this->cloudBusy.store(true);
    this->setCloudStatusText("thomaz/saves/cloud_uploading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    ISaveService* svc   = this->saveService;
    std::uint64_t tid   = this->title.title_id;
    std::string label   = this->title.name;
    // Capture the cancellation flag by value BEFORE dispatch (CONC-03).
    auto cancelled = this->cancelledFlag();

    // push_result: first=blobEmpty, second=CloudPush (valid only when !first)
    auto push_result = std::make_shared<std::pair<bool, CloudPush>>();
    this->runAsync(
        [c, svc, token, tid, label, revision, push_result, cancelled]() {
            std::string err;
            std::vector<std::uint8_t> blob = svc->packageActiveSave(tid, &err);
            if (blob.empty()) {
                push_result->first = true; // blob empty — signal error path
                return;
            }
            push_result->first  = false;
            push_result->second = c->push(token, tid, blob, label, revision, cancelled);
        },
        [this, push_result, tid]() {
            if (push_result->first) {
                // blob packaging failed
                this->cloudBusy.store(false);
                this->setCloudStatusText("thomaz/saves/cloud_err_generic"_i18n);
                brls::Application::notify("thomaz/saves/cloud_err_generic"_i18n);
                return;
            }
            const CloudPush& r = push_result->second;
            this->cloudBusy.store(false);
            if (r.ok) {
                this->cloudConflictRetries = 0; // WR-05: clean push resets the budget
                save_synced_revision(tid, r.newRevision);
                brls::Application::notify("thomaz/saves/cloud_upload_ok"_i18n);
                this->refreshCloudStatus();
                return;
            }
            if (r.conflict) {
                // WR-05: cap the automatic conflict→re-upload chain so a backend
                // that never reconciles cannot spin an unbounded request loop.
                if (this->cloudConflictRetries >= kMaxConflictRetries) {
                    this->cloudConflictRetries = 0;
                    this->setCloudStatusText("thomaz/saves/cloud_err_generic"_i18n);
                    brls::Application::notify("thomaz/saves/cloud_err_generic"_i18n);
                    return;
                }
                ++this->cloudConflictRetries;
                this->doUpload(/*autoRetry=*/true); // lost a race — re-fetches status; may re-prompt if still behind
                return;
            }
            auto errText = this->cloudErrorText(r.error);
            if (r.error == kCloudAuthExpired) this->showCloudLoggedOut();
            this->setCloudStatusText(errText);
            brls::Application::notify(errText);
        });
}

void SaveDetailActivity::doDownload() {
    if (this->cloudBusy.load()) return;
    if (!this->requireSession()) return;
    this->cloudBusy.store(true);
    this->setCloudStatusText("thomaz/saves/cloud_downloading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    ISaveService* svc   = this->saveService;
    std::uint64_t tid   = this->title.title_id;
    // Capture the cancellation flag by value BEFORE dispatch (CONC-03).
    auto cancelled = this->cancelledFlag();

    struct DownloadResult {
        CloudPull pull;
        std::string importErr;
        bool imported = false;
        core::BackupEntry newEntry;
        bool haveNew = false;
    };
    auto dl = std::make_shared<DownloadResult>();
    this->runAsync(
        [c, svc, token, tid, dl, cancelled]() {
            dl->pull = c->pull(token, tid, cancelled);
            if (dl->pull.ok && dl->pull.exists) {
                // Snapshot existing backups so we can pin EXACTLY the one the import
                // creates — robust even if the system clock was set back (newest-by-
                // timestamp could otherwise resolve to an older, future-dated backup).
                std::set<std::string> beforeTs;
                for (const auto& b : core::list_backups(core::saves_root(), tid))
                    beforeTs.insert(b.timestamp);
                dl->imported = svc->importPackageAsBackup(tid, dl->pull.blob, &dl->importErr);
                if (dl->imported) {
                    for (const auto& b : core::list_backups(core::saves_root(), tid)) {
                        if (beforeTs.find(b.timestamp) == beforeTs.end()) {
                            dl->newEntry = b;
                            dl->haveNew  = true;
                            break;
                        }
                    }
                }
            }
        },
        [this, dl, tid]() {
            this->cloudBusy.store(false);
            if (!dl->pull.ok) {
                if (dl->pull.error == kCloudAuthExpired) this->showCloudLoggedOut();
                this->setCloudStatusText(this->cloudErrorText(dl->pull.error));
                return;
            }
            if (!dl->pull.exists) {
                this->setCloudStatusText("thomaz/saves/cloud_status_none"_i18n);
                return;
            }
            if (!dl->imported) {
                this->setCloudStatusText("thomaz/saves/cloud_err_generic"_i18n);
                brls::Application::notify(dl->importErr.empty()
                    ? "thomaz/saves/cloud_err_generic"_i18n : dl->importErr);
                return;
            }
            save_synced_revision(tid, dl->pull.revision);
            brls::Application::notify("thomaz/saves/cloud_download_ok"_i18n);
            this->refreshHistory();      // the new backup now shows in the list
            this->refreshCloudStatus();  // now in sync

            // Offer to restore the backup we just imported (pinned above). If we
            // couldn't identify it, skip the prompt rather than risk restoring the
            // wrong save — the user can still restore manually from the history.
            if (dl->haveNew) {
                core::BackupEntry entry = dl->newEntry;
                brls::Dialog* dlg = new brls::Dialog("thomaz/saves/cloud_restore_q"_i18n);
                dlg->addButton("thomaz/saves/action_restore"_i18n, [this, alive = this->alive, entry]() {
                    if (!alive->load()) return;
                    this->performRestore(entry);
                });
                dlg->addButton("brls/hints/back"_i18n, []() {});
                dlg->open();
            }
        });
}

void SaveDetailActivity::setCloudStatusText(const std::string& text) {
    if (auto* lbl = dynamic_cast<brls::Label*>(this->getView("cloudStatus")))
        lbl->setText(text);
}

void SaveDetailActivity::showCloudLoggedOut() {
    if (auto* btns = this->getView("cloudButtons"))
        btns->setVisibility(brls::Visibility::GONE);
    if (auto* login = this->getView("cloudLogin"))
        login->setVisibility(brls::Visibility::VISIBLE);
    this->setCloudStatusText("");
}

void SaveDetailActivity::promptLogin() {
    auto alive = this->alive; // copy before push: the lambda must not touch `this` until guarded
    brls::Application::pushActivity(new AuthActivity(this->feed, [this, alive]() {
        if (!alive->load()) return;
        if (auto* login = this->getView("cloudLogin"))
            login->setVisibility(brls::Visibility::GONE);
        if (auto* btns = this->getView("cloudButtons"))
            btns->setVisibility(brls::Visibility::VISIBLE);
        this->refreshCloudStatus();
    }));
}

bool SaveDetailActivity::requireSession() {
    // has_value() only proves a session is stored, not that its token is still
    // valid; an expired token returns 401 downstream and we re-prompt then.
    if (load_session().has_value())
        return true;
    this->promptLogin();
    return false;
}

std::string SaveDetailActivity::cloudErrorText(const std::string& apiError) const {
    if (apiError == kCloudAuthExpired)   return "thomaz/saves/cloud_err_auth"_i18n;
    if (apiError == "save_too_large")    return "thomaz/saves/cloud_err_toobig"_i18n;
    if (apiError.rfind("http_", 0) == 0) return "thomaz/saves/cloud_err_generic"_i18n;
    if (apiError.empty())                return "thomaz/saves/cloud_err_network"_i18n;
    return "thomaz/saves/cloud_err_generic"_i18n;
}

void SaveDetailActivity::refreshCloudStatus() {
    if (auto* btns = this->getView("cloudButtons"))
        btns->setVisibility(brls::Visibility::VISIBLE);
    if (auto* login = this->getView("cloudLogin"))
        login->setVisibility(brls::Visibility::GONE);
    this->setCloudStatusText("thomaz/saves/cloud_status_loading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    std::uint64_t tid   = this->title.title_id;

    auto status = std::make_shared<CloudStatus>();
    this->runAsync(
        [c, token, tid, status]() {
            *status = c->getStatus(token, tid);
        },
        [this, status, tid]() {
            if (!status->ok) {
                if (status->error == kCloudAuthExpired) this->showCloudLoggedOut();
                this->setCloudStatusText(this->cloudErrorText(status->error));
                return;
            }
            int synced = load_synced_revision(tid);
            core::SyncSituation sit = core::classify(status->exists, status->revision, synced);
            std::string text;
            if (sit == core::SyncSituation::NoCloud) {
                text = "thomaz/saves/cloud_status_none"_i18n;
            } else {
                std::string key = (sit == core::SyncSituation::CloudAhead)
                                      ? "thomaz/saves/cloud_status_ahead"_i18n
                                      : "thomaz/saves/cloud_status_synced"_i18n;
                auto pos = key.find("{{n}}");
                if (pos != std::string::npos)
                    key.replace(pos, 5, std::to_string(status->revision));
                text = key;
            }
            this->setCloudStatusText(text);
        });
}

} // namespace thomaz
