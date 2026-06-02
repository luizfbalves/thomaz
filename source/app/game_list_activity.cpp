/*
    thomaz — game list activity implementation.
*/

#include "app/game_list_activity.hpp"
#include "app/cheat_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <optional>
#include <set>
#include <string>

#include "core/cheat_db.hpp"
#include "core/db_paths.hpp"
#include "platform/cheat_store.hpp"

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

GameListActivity::GameListActivity(ITitleService* titleService, IHttpClient* http)
    : titleService(titleService), http(http)
{
}

GameListActivity::~GameListActivity()
{
    *this->alive = false; // stop a still-running index load from touching us
}

void GameListActivity::onContentAvailable()
{
    brls::Box* listBox = (brls::Box*)this->getView("gameListBox");
    brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");

    auto titles = this->titleService->listInstalled();

    if (titles.empty())
    {
        // Show empty state, hide the list container.
        if (emptyLabel)
            emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        if (listBox)
            listBox->setVisibility(brls::Visibility::GONE);
        return;
    }

    // Hide empty label.
    if (emptyLabel)
        emptyLabel->setVisibility(brls::Visibility::GONE);

    if (!listBox)
        return;

    for (const auto& title : titles)
    {
        // Build a focusable row: a Box with a name label and a version label.
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setWidth(brls::View::AUTO);
        row->setHeight(64.0f);
        row->setFocusable(true);
        row->setMarginBottom(4.0f);
        row->setPadding(12.0f, 20.0f, 12.0f, 20.0f);
        row->setBackgroundColor(nvgRGB(0x1E, 0x20, 0x27));
        row->setCornerRadius(8.0f);
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
            placeholder->setBackgroundColor(nvgRGB(0x2A, 0x2D, 0x36));
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

        // "Has cheats" badge — hidden until the db index loads (see below).
        brls::Box* hasCheatBadge = makeBadge("thomaz/games/badge_has_cheats"_i18n,
                                             nvgRGB(0x7C, 0x5C, 0xFF), nvgRGB(0xFF, 0xFF, 0xFF));
        hasCheatBadge->setVisibility(brls::Visibility::GONE);
        row->addView(hasCheatBadge);
        this->hasCheatBadges.emplace_back(title.title_id, hasCheatBadge);

        // "Active" badge — a cheat file is already present on the SD for this game.
        if (dir_has_nonempty_txt(core::sd_cheats_dir(title.title_id)))
        {
            row->addView(makeBadge("thomaz/games/badge_active"_i18n,
                                   nvgRGB(0x2E, 0x7D, 0x46), nvgRGB(0xFF, 0xFF, 0xFF)));
        }

        // Version (formatted as decimal).
        brls::Label* versionLabel = new brls::Label();
        versionLabel->setWidth(brls::View::AUTO);
        versionLabel->setHeight(brls::View::AUTO);
        std::string versionStr = "v" + std::to_string(title.version);
        versionLabel->setText(versionStr);
        versionLabel->setFontSize(14.0f);
        versionLabel->setMarginLeft(12.0f);
        row->addView(versionLabel);

        // Tapping opens the cheat detail screen for this game.
        InstalledTitle rowTitle = title;
        IHttpClient* client      = this->http;
        row->registerClickAction([rowTitle, client](brls::View* view) {
            brls::Application::pushActivity(new CheatDetailActivity(rowTitle, client));
            return true;
        });
        // Respond to touch (Switch) and mouse (desktop), not just the A button.
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));

        listBox->addView(row);
    }

    // Reveal "has cheats" badges once the db index is available.
    this->loadCheatIndexAsync();
}

void GameListActivity::loadCheatIndexAsync()
{
    if (this->hasCheatBadges.empty())
        return;

    IHttpClient* client = this->http;
    auto alive          = this->alive;

    brls::async([this, client, alive]() {
        // Use the cached index if we have one; otherwise download + cache it.
        std::string cachePath          = index_cache_path();
        std::optional<std::string> doc = read_text_file(cachePath);
        if (!doc)
        {
            HttpResponse r = client->get(core::db_index_url());
            if (r.ok())
            {
                write_text_file(cachePath, r.body);
                doc = r.body;
            }
        }

        std::set<std::uint64_t> covered;
        if (doc)
            covered = core::parse_db_index(*doc);

        // Back on the UI thread: reveal badges for covered titles.
        brls::sync([this, alive, covered]() {
            if (!alive->load())
                return;
            for (auto& [titleId, badge] : this->hasCheatBadges)
                if (badge && covered.count(titleId))
                    badge->setVisibility(brls::Visibility::VISIBLE);
        });
    });
}

} // namespace thomaz
