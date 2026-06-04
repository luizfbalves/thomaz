/*
    thomaz — shared game-detail left panel (icon + metadata). See game_panel.hpp.
*/

#include "app/game_panel.hpp"
#include "platform/game_stats.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/image.hpp>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>

using namespace brls::literals;

namespace thomaz {

namespace {

// Bytes -> "32.0 MB" style. 0 renders as the unknown dash.
std::string human_size(std::uint64_t bytes)
{
    if (bytes == 0)
        return "thomaz/gameinfo/unknown"_i18n;
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double v   = (double)bytes;
    int    idx = 0;
    while (v >= 1024.0 && idx < 4) {
        v /= 1024.0;
        idx++;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), (idx == 0 ? "%.0f %s" : "%.1f %s"), v, units[idx]);
    return buf;
}

// 0x0100000000010000 -> "0100000000010000".
std::string hex_title_id(std::uint64_t id)
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)id);
    return buf;
}

// Minutes -> "21h 27m" / "47m".
std::string human_playtime(std::uint32_t minutes)
{
    std::uint32_t h = minutes / 60;
    std::uint32_t m = minutes % 60;
    char buf[32];
    if (h > 0)
        std::snprintf(buf, sizeof(buf), "%uh %02um", h, m);
    else
        std::snprintf(buf, sizeof(buf), "%um", m);
    return buf;
}

// POSIX seconds -> "03/06/2024 17:19". 0 renders as the unknown dash.
std::string human_date(std::uint64_t posix)
{
    if (posix == 0)
        return "thomaz/gameinfo/unknown"_i18n;
    std::time_t t = (std::time_t)posix;
    std::tm     tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tm);
    return buf;
}

// First visible letter of the name, uppercased; "?" when empty.
std::string initial_of(const std::string& name)
{
    for (char c : name) {
        if ((unsigned char)c > ' ')
            return std::string(1, (char)std::toupper((unsigned char)c));
    }
    return "?";
}

void set_label(brls::Activity* root, const char* id, const std::string& text)
{
    if (auto* lbl = (brls::Label*)root->getView(id))
        lbl->setText(text);
}

} // namespace

void populate_game_panel(brls::Activity* root, const InstalledTitle& title)
{
    if (!root)
        return;

    // Icon vs. placeholder: show exactly one.
    auto* icon        = (brls::Image*)root->getView("gameIcon");
    auto* placeholder = root->getView("gameIconPlaceholder");
    if (!title.icon.empty() && icon) {
        icon->setImageFromMem(title.icon.data(), (int)title.icon.size());
        icon->setVisibility(brls::Visibility::VISIBLE);
        if (placeholder)
            placeholder->setVisibility(brls::Visibility::GONE);
    } else {
        if (icon)
            icon->setVisibility(brls::Visibility::GONE);
        if (placeholder)
            placeholder->setVisibility(brls::Visibility::VISIBLE);
        set_label(root, "gameIconLetter", initial_of(title.name));
    }

    set_label(root, "gamePanelName", title.name);

    set_label(root, "metaPublisher",
              title.author.empty() ? "thomaz/gameinfo/unknown"_i18n : title.author);
    set_label(root, "metaVersion",
              title.display_version.empty() ? "thomaz/gameinfo/unknown"_i18n : title.display_version);
    set_label(root, "metaSaveSize", human_size(title.save_data_size));
    set_label(root, "metaTitleId", hex_title_id(title.title_id));

    // Play statistics (lazy, single IPC query).
    GameStats stats = query_game_stats(title.title_id);
    if (stats.valid && stats.launches > 0) {
        set_label(root, "metaPlaytime", human_playtime(stats.play_minutes));
        set_label(root, "metaLastPlayed", human_date(stats.last_played));
    } else {
        set_label(root, "metaPlaytime", "thomaz/gameinfo/never_played"_i18n);
        set_label(root, "metaLastPlayed", "thomaz/gameinfo/unknown"_i18n);
    }
}

} // namespace thomaz
