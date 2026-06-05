/*
    thomaz — system (sysmodules) activity implementation.
*/

#include "app/system_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "platform/sysmod/system_reboot.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <utility>

using namespace brls::literals;

namespace thomaz {

SystemActivity::SystemActivity(std::shared_ptr<ISysmoduleStore> store)
    : store(std::move(store))
{
}

void SystemActivity::onContentAvailable()
{
    install_system_status(this);
    install_header_username(this);
    install_tls_warning_banner(this);

    if (brls::View* btn = this->getView("rebootButton"))
    {
        btn->registerClickAction([](brls::View*) {
            if (!system_reboot_to_payload())
                brls::Application::notify("system/reboot_failed"_i18n);
            return true;
        });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }

    this->refreshList();
}

void SystemActivity::refreshList()
{
    auto* listBox = dynamic_cast<brls::Box*>(this->getView("sysmodListBox"));
    if (!listBox)
        return;

    listBox->clearViews();

    std::vector<core::Sysmodule> mods = this->store->list();
    if (mods.empty())
    {
        auto* empty = new brls::Label();
        empty->setText("system/empty"_i18n);
        empty->setFontSize(16.0f);
        listBox->addView(empty);
        return;
    }

    for (const core::Sysmodule& mod : mods)
    {
        std::string title = mod.name;
        if (!mod.has_metadata)
            title += "  (" + "system/no_metadata"_i18n + ")";

        core::Sysmodule captured = mod;

        // Fork uses brls::BooleanCell; initial state + change callback are set
        // through init(title, on, onChange). isOn() reads back the state.
        auto* cell = new brls::BooleanCell();
        cell->init(title, mod.enabled, [this, captured](bool value) {
            this->onToggle(captured, value);
        });
        // Respond to touch (Switch) and mouse (desktop), not just the A button.
        cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));

        listBox->addView(cell);
    }
}

void SystemActivity::onToggle(const core::Sysmodule& mod, bool enabled)
{
    if (!this->store->setEnabled(mod.program_id, enabled))
    {
        brls::Application::notify("system/toggle_failed"_i18n);
        this->refreshList();
        return;
    }
    if (mod.requires_reboot)
        this->showRebootBanner();
}

void SystemActivity::showRebootBanner()
{
    this->rebootPending = true;
    if (brls::View* banner = this->getView("rebootBanner"))
        banner->setVisibility(brls::Visibility::VISIBLE);
}

} // namespace thomaz
