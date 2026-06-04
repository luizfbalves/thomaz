#pragma once
#include <optional>
#include <string>
#include <vector>

namespace thomaz {

// CFW LayeredFS "contents" root. Switch: Atmosphère's
// /atmosphere/contents; desktop: a local "themes-out/contents" for smoke tests.
std::string layeredfs_root();

// Folder holding pre-extracted base firmware layouts (.szs). Switch:
// /themes/systemData (NXThemes Installer writes them there); desktop: local.
std::string base_layout_dir();

// A theme target maps to a system title + the SZS file inside its romfs/lyt.
struct TargetMap {
    std::string title_id;  // 16-hex, e.g. "0100000000001000"
    std::string szs;       // e.g. "ResidentMenu.szs"
};

// Map a Themezer applet target ("ResidentMenu", "Entrance", "Flaunch", "Set",
// "Notification", "Psl", "MyPage") to its title + szs. nullopt if unknown/empty.
std::optional<TargetMap> target_map(const std::string& target);

// base_layout_dir()/<szs>  — where the original layout must already exist.
// Empty string if target is unknown.
std::string base_szs_path(const std::string& target);

// layeredfs_root()/<title>/romfs/lyt/<szs>  — where we write the patched szs.
// Empty string if target is unknown.
std::string output_szs_path(const std::string& target);

// True only if EVERY target is known AND its base szs already exists on disk.
bool base_present_for(const std::vector<std::string>& targets);

} // namespace thomaz
