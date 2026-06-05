/*
    thomaz — mod manager activity implementation.
*/

#include "app/mod_manager_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/game_panel.hpp"
#include "app/mod_browser_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <cctype>
#include <dirent.h>
#include <string>
#include <utility>
#include <vector>

#include "core/mods/mod_paths.hpp"
#include "platform/http_client.hpp"
#include "platform/mods/mod_actions.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {

// True when `name` ends (case-insensitive) in one of the mod archive suffixes.
bool is_archive_name(const std::string& name)
{
    auto ends_with = [&](const char* ext) {
        std::string s = name;
        for (auto& c : s)
            c = (char)std::tolower((unsigned char)c);
        std::string e = ext;
        return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0;
    };
    return ends_with(".zip") || ends_with(".7z") || ends_with(".rar");
}

// filename without its final extension ("Mod.v2.zip" -> "Mod.v2").
std::string strip_extension(const std::string& name)
{
    std::string::size_type dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return name;
    return name.substr(0, dot);
}

} // namespace

ModManagerActivity::ModManagerActivity(InstalledTitle title, IHttpClient* http)
    : title(std::move(title)), http(http)
{
}

void ModManagerActivity::onContentAvailable()
{
    install_header_username(this);
    install_tls_warning_banner(this);
    install_help_action(this, "modFrame", "thomaz/help/mods");

    populate_game_panel(this, this->title);

    // installed_mods() is a fast local SD dir read, so build inline.
    this->refreshList();
}

void ModManagerActivity::refreshList()
{
    auto* listBox    = (brls::Box*)this->getView("modListBox");
    auto* emptyLabel = (brls::Label*)this->getView("emptyLabel");

    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE);

    if (!listBox)
        return;

    listBox->clearViews();

    // "Import from SD" button (mirrors clear_cheats' clear button Box).
    {
        auto* importBtn = new brls::Box(brls::Axis::ROW);
        importBtn->setHeight(56.0f);
        importBtn->setFocusable(true);
        importBtn->setMarginBottom(16.0f);
        importBtn->setCornerRadius(8.0f);
        importBtn->setJustifyContent(brls::JustifyContent::CENTER);
        importBtn->setAlignItems(brls::AlignItems::CENTER);
        importBtn->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2
        auto* importLabel = new brls::Label();
        importLabel->setText("mods/import"_i18n);
        importLabel->setFontSize(18.0f);
        importLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        importBtn->addView(importLabel);
        importBtn->registerClickAction([this, alive = this->alive](brls::View*) {
            brls::sync([this, alive]() { if (!alive->load()) return; this->importFlow(); });
            return true;
        });
        importBtn->addGestureRecognizer(new brls::TapGestureRecognizer(importBtn));
        listBox->addView(importBtn);
    }

    // "Get mods (GameBanana)" button (mirrors the Import button).
    {
        auto* browseBtn = new brls::Box(brls::Axis::ROW);
        browseBtn->setHeight(56.0f);
        browseBtn->setFocusable(true);
        browseBtn->setMarginBottom(16.0f);
        browseBtn->setCornerRadius(8.0f);
        browseBtn->setJustifyContent(brls::JustifyContent::CENTER);
        browseBtn->setAlignItems(brls::AlignItems::CENTER);
        browseBtn->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2
        auto* browseLabel = new brls::Label();
        browseLabel->setText("mods/browse"_i18n);
        browseLabel->setFontSize(18.0f);
        browseLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        browseBtn->addView(browseLabel);
        browseBtn->registerClickAction([this](brls::View*) {
            brls::Application::pushActivity(new ModBrowserActivity(this->title, this->http));
            return true;
        });
        browseBtn->addGestureRecognizer(new brls::TapGestureRecognizer(browseBtn));
        listBox->addView(browseBtn);
    }

    std::vector<core::StagedMod> mods = installed_mods(this->title.title_id);

    if (mods.empty())
    {
        if (emptyLabel)
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        return; // keep the import button visible
    }
    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);

    std::uint64_t tid = this->title.title_id;

    for (const auto& mod : mods)
    {
        std::string modName = mod.name;

        // Active toggle for this mod.
        auto* cell = new brls::BooleanCell();
        cell->init(modName, mod.active, [this, alive = this->alive, tid, modName, cell](bool on) {
            if (on)
            {
                ModActionResult r = enable_mod(tid, modName);
                if (!r.ok)
                {
                    brls::Application::notify("mods/enable_failed"_i18n);
                    cell->setOn(false, false);
                    return;
                }
            }
            else
            {
                ModActionResult r = disable_mod(tid);
                if (!r.ok)
                {
                    brls::Application::notify("mods/disable_failed"_i18n);
                    cell->setOn(true, false);
                    return;
                }
            }
            // Reflect one-active-per-game across rows.
            brls::sync([this, alive]() { if (!alive->load()) return; this->refreshList(); });
        });
        cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
        listBox->addView(cell);

        // Per-mod uninstall button row.
        auto* uninstallBtn = new brls::Box(brls::Axis::ROW);
        uninstallBtn->setHeight(44.0f);
        uninstallBtn->setFocusable(true);
        uninstallBtn->setMarginBottom(12.0f);
        uninstallBtn->setCornerRadius(8.0f);
        uninstallBtn->setJustifyContent(brls::JustifyContent::CENTER);
        uninstallBtn->setAlignItems(brls::AlignItems::CENTER);
        uninstallBtn->setBackgroundColor(nvgRGB(0xC0, 0x3A, 0x3A));
        auto* uninstallLabel = new brls::Label();
        uninstallLabel->setText("mods/uninstall"_i18n);
        uninstallLabel->setFontSize(15.0f);
        uninstallLabel->setTextColor(nvgRGB(0xFF, 0xFF, 0xFF));
        uninstallBtn->addView(uninstallLabel);
        uninstallBtn->registerClickAction([this, tid, modName](brls::View*) {
            auto* dialog = new brls::Dialog("mods/uninstall_confirm"_i18n);
            dialog->addButton("mods/uninstall_button"_i18n, [this, alive = this->alive, tid, modName]() {
                if (!alive->load()) return;
                ModActionResult r = uninstall_mod(tid, modName);
                if (!r.ok)
                    brls::Application::notify("mods/uninstall_failed"_i18n);
                this->refreshList();
            });
            dialog->addButton("mods/cancel"_i18n, []() {});
            dialog->open();
            return true;
        });
        uninstallBtn->addGestureRecognizer(new brls::TapGestureRecognizer(uninstallBtn));
        listBox->addView(uninstallBtn);
    }
}

void ModManagerActivity::importFlow()
{
    std::string incoming = core::mod_staging_root() + "/_incoming";

    std::vector<std::string> archives; // file names only
    if (DIR* d = ::opendir(incoming.c_str()))
    {
        while (struct dirent* e = ::readdir(d))
        {
            std::string name = e->d_name;
            if (name == "." || name == "..")
                continue;
            if (is_archive_name(name))
                archives.push_back(name);
        }
        ::closedir(d);
    }

    if (archives.empty())
    {
        brls::Application::notify("mods/no_incoming"_i18n);
        return;
    }

    auto* listBox = (brls::Box*)this->getView("modListBox");
    if (!listBox)
        return;

    auto* emptyLabel = (brls::Label*)this->getView("emptyLabel");
    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);

    listBox->clearViews();

    // Header row: "Pick an archive".
    {
        auto* header = new brls::Label();
        header->setText("mods/pick_archive"_i18n);
        header->setFontSize(18.0f);
        header->setMarginBottom(12.0f);
        listBox->addView(header);
    }

    // One tappable row per incoming archive.
    for (const auto& name : archives)
    {
        std::string fullPath = incoming + "/" + name;
        std::string modName  = strip_extension(name);

        auto* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(48.0f);
        row->setFocusable(true);
        row->setMarginBottom(8.0f);
        row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        row->setCornerRadius(12.0f);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23)); // surface_1
        auto* label = new brls::Label();
        label->setText(name);
        label->setFontSize(16.0f);
        label->setGrow(1.0f);
        row->addView(label);
        row->registerClickAction([this, fullPath, modName](brls::View*) {
            brls::sync([this, fullPath, modName]() { this->doImport(fullPath, modName); });
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        listBox->addView(row);
    }

    // Back row → return to the staged-mod list.
    {
        auto* backRow = new brls::Box(brls::Axis::ROW);
        backRow->setHeight(48.0f);
        backRow->setFocusable(true);
        backRow->setMarginTop(8.0f);
        backRow->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        backRow->setCornerRadius(12.0f);
        backRow->setAlignItems(brls::AlignItems::CENTER);
        backRow->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2
        auto* backLabel = new brls::Label();
        backLabel->setText("mods/cancel"_i18n);
        backLabel->setFontSize(16.0f);
        backLabel->setGrow(1.0f);
        backRow->addView(backLabel);
        backRow->registerClickAction([this, alive = this->alive](brls::View*) {
            brls::sync([this, alive]() { if (!alive->load()) return; this->refreshList(); });
            return true;
        });
        backRow->addGestureRecognizer(new brls::TapGestureRecognizer(backRow));
        listBox->addView(backRow);
    }
}

void ModManagerActivity::doImport(const std::string& archive_path, const std::string& mod_name)
{
    ModActionResult r = import_archive(this->title.title_id, mod_name, archive_path, nullptr);
    if (r.ok)
        brls::Application::notify("mods/imported"_i18n);
    else
        brls::Application::notify("mods/import_failed"_i18n + std::string(": ") + r.error);
    this->refreshList();
}

} // namespace thomaz
