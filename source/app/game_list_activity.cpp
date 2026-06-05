/*
    thomaz — game list activity implementation.
*/

#include "app/game_list_activity.hpp"
#include "app/app_header.hpp"
#include "app/tls_banner.hpp"
#include "app/cheat_detail_activity.hpp"
#include "app/clear_cheats_activity.hpp"
#include "app/mod_manager_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <optional>
#include <set>
#include <string>

#include "core/cheat_db.hpp"
#include "core/db_paths.hpp"
#include "core/title_filter.hpp"
#include "platform/cheat_store.hpp"
#include "platform/title_visibility_store.hpp"

using namespace brls::literals;

namespace thomaz {

namespace {

// A small pill badge (a padded, rounded, colored Box wrapping a Label).
brls::Box* makeBadge(const std::string& text, NVGcolor bg, NVGcolor fg)
{
    auto* box = new brls::Box(brls::Axis::ROW);
    box->setBackgroundColor(bg);
    box->setCornerRadius(6.0f);
    box->setPadding(3.0f, 9.0f, 3.0f, 9.0f);
    box->setMarginLeft(8.0f);
    box->setAlignItems(brls::AlignItems::CENTER);

    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(13.0f);
    label->setTextColor(fg);
    box->addView(label);
    return box;
}

} // namespace

GameListActivity::GameListActivity(ITitleService* titleService, IHttpClient* http, Target target)
    : titleService(titleService), http(http), target(target)
{
}

void GameListActivity::onContentAvailable()
{
    install_header_username(this);
    install_tls_warning_banner(this);
    install_help_action(this, "gameListFrame",
                        this->target == Target::Mods ? "thomaz/help/mods_list"
                                                      : "thomaz/help/cheats_list");

    // Load persisted visibility preferences.
    store_.load();

    // Button X on the frame: toggle global show_hidden → rebuild list.
    if (auto* frame = this->getView("gameListFrame")) {
        frame->registerAction(
            "thomaz/games/toggle_show_hidden"_i18n, brls::BUTTON_X,
            [this](brls::View*) {
                store_.toggle_show_hidden();
                store_.save();
                this->rebuildList();
                return true;
            }, false);
    }

    // Reading each installed title's control data (name + 128KB icon) off the
    // NAND is slow enough to freeze the push animation if done inline. Do it on
    // a worker thread while the XML spinner shows, then build the rows on the UI
    // thread. Mirrors the async pattern already used for the cheats-db index.
    ITitleService* svc = this->titleService;

    auto titles = std::make_shared<std::vector<InstalledTitle>>();
    this->runAsync(
        [svc, titles]() {
            *titles = svc->listInstalled();
        },
        [this, titles]() {
            allTitles_ = *titles;
            this->rebuildList();
        });
}

void GameListActivity::rebuildList()
{
    auto* listBox = dynamic_cast<brls::Box*>(this->getView("gameListBox"));
    auto* emptyLabel = dynamic_cast<brls::Label*>(this->getView("emptyLabel"));
    if (!listBox || !emptyLabel)
        return;

    // Hide spinner once data is available (idempotent — GONE on already-GONE is a no-op).
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE);

    // Clear existing rows and cheat-badge tracking.
    listBox->clearViews(true);
    this->hasCheatBadges.clear();

    // Entry to the "clear cheats" screen (cheats mode only) — always at top.
    if (this->target == Target::Cheats)
    {
        ITitleService* svc = this->titleService;
        brls::Box* clearEntry = new brls::Box(brls::Axis::ROW);
        clearEntry->setHeight(48.0f);
        clearEntry->setFocusable(true);
        clearEntry->setMarginBottom(12.0f);
        clearEntry->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        clearEntry->setCornerRadius(12.0f);
        clearEntry->setAlignItems(brls::AlignItems::CENTER);
        clearEntry->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2

        brls::Label* clearLabel = new brls::Label();
        clearLabel->setText("thomaz/clear/entry"_i18n);
        clearLabel->setFontSize(16.0f);
        clearLabel->setGrow(1.0f);
        clearEntry->addView(clearLabel);

        clearEntry->registerClickAction([svc](brls::View*) {
            brls::Application::pushActivity(new ClearCheatsActivity(svc));
            return true;
        });
        clearEntry->addGestureRecognizer(new brls::TapGestureRecognizer(clearEntry));
        listBox->addView(clearEntry);
    }

    int visibleCount = 0;

    for (const auto& title : allTitles_)
    {
        bool eh = core::effectively_hidden(title, store_.force_hidden(), store_.force_shown());

        // When show_hidden is off, skip entries that are effectively hidden.
        if (!store_.show_hidden() && eh)
            continue;

        ++visibleCount;

        // Build a focusable row: a Box with a name label and a version label.
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setWidth(brls::View::AUTO);
        row->setHeight(64.0f);
        row->setFocusable(true);
        row->setMarginBottom(4.0f);
        row->setPadding(12.0f, 20.0f, 12.0f, 20.0f);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23)); // surface_1
        row->setCornerRadius(12.0f);
        row->setAlignItems(brls::AlignItems::CENTER);

        // Game icon (JPEG from control data) or a placeholder square.
        if (!title.icon.empty())
        {
            brls::Image* icon = new brls::Image();
            icon->setWidth(48.0f);
            icon->setHeight(48.0f);
            icon->setCornerRadius(8.0f);
            icon->setScalingType(brls::ImageScalingType::FILL);
            icon->setMarginRight(16.0f);
            icon->setImageFromMem(title.icon.data(), (int)title.icon.size());
            row->addView(icon);
        }
        else
        {
            brls::Box* placeholder = new brls::Box();
            placeholder->setWidth(48.0f);
            placeholder->setHeight(48.0f);
            placeholder->setCornerRadius(8.0f);
            placeholder->setMarginRight(16.0f);
            placeholder->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D)); // surface_2
            row->addView(placeholder);
        }

        // Game name (grows).
        brls::Label* nameLabel = new brls::Label();
        nameLabel->setWidth(brls::View::AUTO);
        nameLabel->setHeight(brls::View::AUTO);
        nameLabel->setGrow(1.0f);
        nameLabel->setText(title.name);
        nameLabel->setFontSize(18.0f);
        row->addView(nameLabel);

        // "Hidden" badge — shown when the title is effectively hidden but show_hidden is on.
        if (eh && store_.show_hidden()) {
            row->addView(makeBadge("thomaz/games/badge_hidden"_i18n,
                                   nvgRGBA(0x80, 0x80, 0x80, 0x40), nvgRGB(0xC0, 0xC0, 0xC0)));
        }

        // Cheats-specific badges (skipped in Mods mode).
        if (this->target == Target::Cheats)
        {
            // "Has cheats" badge — hidden until the db index loads (see below).
            brls::Box* hasCheatBadge = makeBadge("thomaz/games/badge_has_cheats"_i18n,
                                                 nvgRGBA(0x7C, 0x5C, 0xFF, 0x29), nvgRGB(0x92, 0x77, 0xFF));
            hasCheatBadge->setVisibility(brls::Visibility::GONE);
            row->addView(hasCheatBadge);
            this->hasCheatBadges.emplace_back(title.title_id, hasCheatBadge);

            // "Active" badge — a cheat file is already present on the SD for this game.
            if (dir_has_nonempty_txt(core::sd_cheats_dir(title.title_id)))
            {
                row->addView(makeBadge("thomaz/games/badge_active"_i18n,
                                       nvgRGBA(0x57, 0xC9, 0x8A, 0x29), nvgRGB(0x57, 0xC9, 0x8A)));
            }
        }

        // Version. Prefer the NACP display string ("1.0.1"); the bare numeric
        // meta version (0, 65536, ...) is opaque — "v0" just means "base game,
        // no update installed". Fall back to it only when the string is absent.
        brls::Label* versionLabel = new brls::Label();
        versionLabel->setWidth(brls::View::AUTO);
        versionLabel->setHeight(brls::View::AUTO);
        std::string versionStr = title.display_version.empty()
            ? ("v" + std::to_string(title.version))
            : ("v" + title.display_version);
        versionLabel->setText(versionStr);
        versionLabel->setFontSize(14.0f);
        versionLabel->setMarginLeft(12.0f);
        row->addView(versionLabel);

        // Tapping opens the detail screen for this game (cheats or mods).
        InstalledTitle rowTitle = title;
        IHttpClient* client      = this->http;
        Target rowTarget         = this->target;
        row->registerClickAction([rowTitle, client, rowTarget](brls::View* view) {
            if (rowTarget == Target::Mods)
                brls::Application::pushActivity(new ModManagerActivity(rowTitle, client));
            else
                brls::Application::pushActivity(new CheatDetailActivity(rowTitle, client));
            return true;
        });
        // Respond to touch (Switch) and mouse (desktop), not just the A button.
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));

        // Button Y per row: toggle the title's visibility override → persist → rebuild.
        InstalledTitle rowTitleCopy = title;
        bool isHidden = eh;
        row->registerAction(
            isHidden ? "thomaz/games/unhide"_i18n : "thomaz/games/hide"_i18n,
            brls::BUTTON_Y,
            [this, rowTitleCopy](brls::View*) {
                store_.toggle_title(rowTitleCopy);
                store_.save();
                this->rebuildList();
                return true;
            }, false);

        listBox->addView(row);
    }

    // Show empty state when no titles are visible; otherwise show the list.
    if (visibleCount == 0 && allTitles_.empty()) {
        emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        listBox->setVisibility(brls::Visibility::GONE);
    } else {
        emptyLabel->setVisibility(brls::Visibility::GONE);
        listBox->setVisibility(brls::Visibility::VISIBLE);
    }

    // Reveal "has cheats" badges once the db index is available.
    this->loadCheatIndexAsync();
}

void GameListActivity::loadCheatIndexAsync()
{
    if (this->hasCheatBadges.empty())
        return;

    IHttpClient* client = this->http;

    auto covered   = std::make_shared<std::set<std::uint64_t>>();
    auto cancelled = this->cancelledFlag();
    this->runAsync(
        [client, covered, cancelled]() {
            // Use the cached index if we have one; otherwise download + cache it.
            std::string cachePath          = index_cache_path();
            std::optional<std::string> doc = read_text_file(cachePath);
            if (!doc)
            {
                HttpRequest req;
                req.url       = core::db_index_url();
                req.cancelled = cancelled;
                HttpResponse r = client->request(req);
                if (r.ok())
                {
                    write_text_file(cachePath, r.body);
                    doc = r.body;
                }
            }
            if (doc)
                *covered = core::parse_db_index(*doc);
        },
        [this, covered]() {
            // Back on the UI thread: reveal badges for covered titles.
            for (auto& [titleId, badge] : this->hasCheatBadges)
                if (badge && covered->count(titleId))
                    badge->setVisibility(brls::Visibility::VISIBLE);
        });
}

} // namespace thomaz
