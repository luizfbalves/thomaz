#include "app/save_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>

#include "core/backup_store.hpp"

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

SaveDetailActivity::SaveDetailActivity(InstalledTitle title, ISaveService* saveService)
    : title(std::move(title)), saveService(saveService)
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

} // namespace thomaz
