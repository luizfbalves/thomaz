#include "app/save_manager_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/save_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>

#include "core/backup_store.hpp"
#include "core/title_filter.hpp"
#include "platform/title_visibility_store.hpp"

using namespace brls::literals;

namespace thomaz {

SaveManagerActivity::SaveManagerActivity(ITitleService* titleService, ISaveService* saveService,
                                         ICloudSaveClient* cloudSaves, IAuthClient* feed)
    : titleService(titleService), saveService(saveService), cloudSaves(cloudSaves), feed(feed)
{
}

void SaveManagerActivity::onContentAvailable()
{
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);
    install_help_action(this, "saveManagerFrame", "thomaz/help/saves_list");

    ITitleService* svc = this->titleService;

    auto titles = std::make_shared<std::vector<InstalledTitle>>();
    this->runAsync(
        [svc, titles]() {
            *titles = svc->listInstalled();
        },
        [this, titles]() {
            this->populate(*titles);
        });
}

void SaveManagerActivity::populate(const std::vector<InstalledTitle>& titles)
{
    auto* listBox = dynamic_cast<brls::Box*>(this->getView("saveListBox"));
    if (!listBox)
    {
        brls::Logger::error("saveListBox missing or not a Box");
        return;
    }
    auto* emptyLabel = dynamic_cast<brls::Label*>(this->getView("emptyLabel"));
    if (!emptyLabel)
    {
        brls::Logger::error("emptyLabel missing or not a Label");
        return;
    }
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE);

    if (titles.empty()) {
        emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        listBox->setVisibility(brls::Visibility::GONE);
        return;
    }

    std::string root          = core::saves_root();
    ISaveService* save        = this->saveService;
    ICloudSaveClient* cloud   = this->cloudSaves;
    IAuthClient* feedClient   = this->feed;

    // Hide non-game homebrew (sphaira & co.) from the save list, mirroring the
    // games list. A NonGame title has no save data, so it has nothing to manage
    // here anyway. Respects the same persisted overrides + global show_hidden
    // toggle the user controls from the games list (single source of truth).
    TitleVisibilityStore visibility;
    visibility.load();

    int shownCount = 0;
    for (const auto& title : titles) {
        if (!visibility.show_hidden() &&
            core::effectively_hidden(title, visibility.force_hidden(), visibility.force_shown()))
            continue;
        ++shownCount;

        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(64.0f);
        row->setFocusable(true);
        row->setMarginBottom(4.0f);
        row->setPadding(12.0f, 20.0f, 12.0f, 20.0f);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));
        row->setCornerRadius(12.0f);
        row->setAlignItems(brls::AlignItems::CENTER);

        if (!title.icon.empty()) {
            brls::Image* icon = new brls::Image();
            icon->setWidth(48.0f); icon->setHeight(48.0f);
            icon->setCornerRadius(8.0f);
            icon->setScalingType(brls::ImageScalingType::FILL);
            icon->setMarginRight(16.0f);
            icon->setImageFromMem(title.icon.data(), (int)title.icon.size());
            row->addView(icon);
        } else {
            brls::Box* ph = new brls::Box();
            ph->setWidth(48.0f); ph->setHeight(48.0f);
            ph->setCornerRadius(8.0f); ph->setMarginRight(16.0f);
            ph->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
            row->addView(ph);
        }

        brls::Box* textCol = new brls::Box(brls::Axis::COLUMN);
        textCol->setGrow(1.0f);
        brls::Label* nameLabel = new brls::Label();
        nameLabel->setText(title.name);
        nameLabel->setFontSize(18.0f);
        textCol->addView(nameLabel);

        brls::Label* dateLabel = new brls::Label();
        auto last = core::last_backup_timestamp(root, title.title_id);
        if (last) {
            std::string s = "thomaz/saves/last_backup"_i18n; // contains literal "{{when}}"
            std::string when = core::format_timestamp_label(*last);
            auto pos = s.find("{{when}}");
            if (pos != std::string::npos)
                s.replace(pos, 8, when);
            else
                s += " " + when;
            dateLabel->setText(s);
        } else {
            dateLabel->setText("thomaz/saves/never"_i18n);
        }
        dateLabel->setFontSize(13.0f);
        dateLabel->setTextColor(nvgRGB(0x8A, 0x8C, 0x99));
        textCol->addView(dateLabel);
        row->addView(textCol);

        InstalledTitle rowTitle = title;
        row->registerClickAction([rowTitle, save, cloud, feedClient](brls::View*) {
            brls::Application::pushActivity(new SaveDetailActivity(rowTitle, save, cloud, feedClient));
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        listBox->addView(row);
    }

    // Everything installed was filtered out (only homebrew present) — surface the
    // empty state rather than a blank list.
    if (shownCount == 0) {
        emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        listBox->setVisibility(brls::Visibility::GONE);
        return;
    }

    // Give focus to the first item in the list (no-op on empty list via guard
    // in base class; also idempotent — only fires once per activity instance).
    this->claimInitialFocus(listBox);
}

} // namespace thomaz
