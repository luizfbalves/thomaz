/*
    thomaz — clear cheats activity implementation.
*/

#include "app/clear_cheats_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <string>

#include "core/db_paths.hpp"
#include "platform/cheat_store.hpp"

using namespace brls::literals;

namespace thomaz {

ClearCheatsActivity::ClearCheatsActivity(ITitleService* titleService)
    : titleService(titleService)
{
}

void ClearCheatsActivity::onContentAvailable()
{
    install_header_username(this);
    install_tls_warning_banner(this);

    // listInstalled() reads control data off the NAND (slow). Run it on a worker
    // thread with the spinner up so the screen transition stays smooth.
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

void ClearCheatsActivity::populate(const std::vector<InstalledTitle>& titles)
{
    auto* listBox    = (brls::Box*)this->getView("clearListBox");
    auto* emptyLabel = (brls::Label*)this->getView("emptyLabel");

    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE); // load finished

    if (titles.empty())
    {
        if (emptyLabel)
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        if (listBox)
            listBox->setVisibility(brls::Visibility::GONE);
        return;
    }
    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);
    if (!listBox)
        return;

    // "Select all" toggle: flips every game checkbox to its state.
    auto* selectAll = new brls::BooleanCell();
    selectAll->init("thomaz/clear/select_all"_i18n, false, [this](bool on) {
        for (auto& [titleId, cell] : this->selections)
            cell->setOn(on, false);
    });
    selectAll->addGestureRecognizer(new brls::TapGestureRecognizer(selectAll));
    listBox->addView(selectAll);

    // One checkbox per game; mark the ones that currently have cheat files.
    for (const auto& title : titles)
    {
        bool active = dir_has_nonempty_txt(core::sd_cheats_dir(title.title_id));
        std::string label = title.name;
        if (active)
            label += "thomaz/clear/active_suffix"_i18n;

        auto* cell = new brls::BooleanCell();
        cell->init(label, false, [](bool) {});
        cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
        listBox->addView(cell);
        this->selections.emplace_back(title.title_id, cell);
    }

    // Destructive "clear selected" button.
    auto* clearBtn = new brls::Box(brls::Axis::ROW);
    clearBtn->setHeight(56.0f);
    clearBtn->setFocusable(true);
    clearBtn->setMarginTop(16.0f);
    clearBtn->setCornerRadius(8.0f);
    clearBtn->setJustifyContent(brls::JustifyContent::CENTER);
    clearBtn->setAlignItems(brls::AlignItems::CENTER);
    clearBtn->setBackgroundColor(nvgRGB(0xC0, 0x3A, 0x3A));
    auto* clearLabel = new brls::Label();
    clearLabel->setText("thomaz/clear/clear_selected"_i18n);
    clearLabel->setFontSize(18.0f);
    clearLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
    clearBtn->addView(clearLabel);
    clearBtn->registerClickAction([this](brls::View*) {
        this->confirmAndClear();
        return true;
    });
    clearBtn->addGestureRecognizer(new brls::TapGestureRecognizer(clearBtn));
    listBox->addView(clearBtn);
}

void ClearCheatsActivity::confirmAndClear()
{
    int count = 0;
    for (auto& [titleId, cell] : this->selections)
        if (cell->isOn())
            ++count;

    if (count == 0)
    {
        brls::Application::notify("thomaz/clear/none_selected"_i18n);
        return;
    }

    std::string msg = "thomaz/clear/confirm_pre"_i18n + std::to_string(count) +
                      "thomaz/clear/confirm_post"_i18n;

    auto* dialog = new brls::Dialog(msg);
    dialog->addButton("thomaz/clear/confirm_button"_i18n, [this]() {
        int cleared = 0;
        for (auto& [titleId, cell] : this->selections)
            if (cell->isOn())
                cleared += clear_cheat_files(core::sd_cheats_dir(titleId));

        (void)cleared;
        brls::Application::notify("thomaz/clear/done"_i18n);
        brls::Application::popActivity(); // back to the game list
    });
    dialog->addButton("thomaz/clear/cancel"_i18n, []() {});
    dialog->open();
}

} // namespace thomaz
