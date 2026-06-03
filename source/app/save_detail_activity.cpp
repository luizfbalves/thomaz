#include "app/save_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>

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
                                       ICloudSaveClient* cloudSaves, IFeedClient* feed)
    : title(std::move(title)), saveService(saveService), cloudSaves(cloudSaves), feed(feed)
{
}

SaveDetailActivity::~SaveDetailActivity()
{
    *this->alive = false;
}

void SaveDetailActivity::onContentAvailable()
{
    if (auto* name = (brls::Label*)this->getView("gameName"))
        name->setText(this->title.name);

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
        login->registerClickAction([this](brls::View*) { this->requireSession(); return true; });
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

    if (auto* lbl = (brls::Label*)this->getView("lastBackup"))
        lbl->setText(lastBackupText(core::last_backup_timestamp(root, this->title.title_id)));

    brls::Box* box = (brls::Box*)this->getView("historyBox");
    if (!box)
        return;
    box->clearViews();

    auto entries = core::list_backups(root, this->title.title_id);
    for (const auto& entry : entries) {
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(52.0f);
        row->setFocusable(true);
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

        brls::Label* action = new brls::Label();
        action->setText("thomaz/saves/action_restore"_i18n);
        action->setFontSize(14.0f);
        action->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
        row->addView(action);

        core::BackupEntry captured = entry;
        row->registerClickAction([this, captured](brls::View*) {
            this->doRestore(captured);
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        box->addView(row);
    }
}

void SaveDetailActivity::doBackup()
{
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);

    ISaveService* svc = this->saveService;
    InstalledTitle t  = this->title;
    auto alive        = this->alive;

    brls::async([this, svc, t, alive]() {
        std::string err;
        bool ok = svc->backup(t, &err);
        brls::sync([this, alive, ok, err]() {
            if (!alive->load())
                return;
            if (auto* spinner = this->getView("spinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            brls::Application::notify(ok ? "thomaz/saves/backup_ok"_i18n
                                         : ("thomaz/saves/backup_fail"_i18n + std::string(": ") + err));
            if (ok)
                this->refreshHistory();
        });
    });
}

void SaveDetailActivity::doRestore(const core::BackupEntry& entry)
{
    auto doIt = [this, entry]() {
        if (!this->alive->load())
            return; // activity gone before the user confirmed
        if (auto* spinner = this->getView("spinner"))
            spinner->setVisibility(brls::Visibility::VISIBLE);
        ISaveService* svc   = this->saveService;
        std::uint64_t tid   = this->title.title_id;
        core::BackupEntry e = entry;
        auto alive          = this->alive;
        brls::async([this, svc, e, tid, alive]() {
            std::string err;
            bool ok = svc->restore(e, tid, &err);
            brls::sync([this, alive, ok, err]() {
                if (!alive->load())
                    return;
                if (auto* spinner = this->getView("spinner"))
                    spinner->setVisibility(brls::Visibility::GONE);
                brls::Application::notify(ok ? "thomaz/saves/restore_ok"_i18n
                                             : ("thomaz/saves/restore_fail"_i18n + std::string(": ") + err));
                if (ok)
                    this->refreshHistory();
            });
        });
    };

    // Destructive — confirm first.
    brls::Dialog* dialog = new brls::Dialog("thomaz/saves/confirm_restore_body"_i18n);
    dialog->addButton("thomaz/saves/action_restore"_i18n, [doIt]() { doIt(); });
    dialog->addButton("brls/hints/back"_i18n, []() {});
    dialog->open();
}

void SaveDetailActivity::doUpload()   { /* Task 12 */ }
void SaveDetailActivity::doDownload() { /* Task 13 */ }

void SaveDetailActivity::setCloudStatusText(const std::string& text) {
    if (auto* lbl = (brls::Label*)this->getView("cloudStatus"))
        lbl->setText(text);
}

void SaveDetailActivity::showCloudLoggedOut() {
    if (auto* btns = this->getView("cloudButtons"))
        btns->setVisibility(brls::Visibility::GONE);
    if (auto* login = this->getView("cloudLogin"))
        login->setVisibility(brls::Visibility::VISIBLE);
    this->setCloudStatusText("");
}

bool SaveDetailActivity::requireSession() {
    if (load_session().has_value())
        return true;
    auto alive = this->alive; // copy before push: the lambda must not touch `this` until guarded
    brls::Application::pushActivity(new AuthActivity(this->feed, [this, alive]() {
        if (!alive->load()) return;
        if (auto* login = this->getView("cloudLogin"))
            login->setVisibility(brls::Visibility::GONE);
        if (auto* btns = this->getView("cloudButtons"))
            btns->setVisibility(brls::Visibility::VISIBLE);
        this->refreshCloudStatus();
    }));
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
    auto alive          = this->alive;

    brls::async([this, c, alive, token, tid]() {
        CloudStatus s = c->getStatus(token, tid);
        brls::sync([this, alive, s, tid]() {
            if (!alive->load()) return;
            if (!s.ok) {
                if (s.error == kCloudAuthExpired) this->showCloudLoggedOut();
                this->setCloudStatusText(this->cloudErrorText(s.error));
                return;
            }
            this->cloudRevision = s.revision;
            int synced = load_synced_revision(tid);
            core::SyncSituation sit = core::classify(s.exists, s.revision, synced);
            std::string text;
            if (sit == core::SyncSituation::NoCloud) {
                text = "thomaz/saves/cloud_status_none"_i18n;
            } else {
                std::string key = (sit == core::SyncSituation::CloudAhead)
                                      ? "thomaz/saves/cloud_status_ahead"_i18n
                                      : "thomaz/saves/cloud_status_synced"_i18n;
                auto pos = key.find("{{n}}");
                if (pos != std::string::npos)
                    key.replace(pos, 5, std::to_string(s.revision));
                text = key;
            }
            this->setCloudStatusText(text);
        });
    });
}

} // namespace thomaz
