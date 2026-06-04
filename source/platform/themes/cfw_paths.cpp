#include "platform/themes/cfw_paths.hpp"
#include <sys/stat.h>

namespace thomaz {

std::string layeredfs_root() {
#ifdef __SWITCH__
    return "/atmosphere/contents";
#else
    return "themes-out/contents";
#endif
}

std::string base_layout_dir() {
#ifdef __SWITCH__
    return "/themes/systemData";
#else
    return "themes/systemData";
#endif
}

std::optional<TargetMap> target_map(const std::string& target) {
    // qlaunch (home menu) hosts most layouts; MyPage/Psl are separate titles.
    if (target == "ResidentMenu") return TargetMap{"0100000000001000", "ResidentMenu.szs"};
    if (target == "Entrance")     return TargetMap{"0100000000001000", "Entrance.szs"};
    if (target == "Flaunch")      return TargetMap{"0100000000001000", "Flaunch.szs"};
    if (target == "Set")          return TargetMap{"0100000000001000", "Set.szs"};
    if (target == "Notification") return TargetMap{"0100000000001000", "Notification.szs"};
    if (target == "Psl")          return TargetMap{"0100000000001007", "Psl.szs"};
    if (target == "MyPage")       return TargetMap{"0100000000001013", "MyPage.szs"};
    return std::nullopt;
}

std::string base_szs_path(const std::string& target) {
    auto m = target_map(target);
    if (!m) return "";
    return base_layout_dir() + "/" + m->szs;
}

std::string output_szs_path(const std::string& target) {
    auto m = target_map(target);
    if (!m) return "";
    return layeredfs_root() + "/" + m->title_id + "/romfs/lyt/" + m->szs;
}

bool base_present_for(const std::vector<std::string>& targets) {
    if (targets.empty()) return false;
    for (const auto& t : targets) {
        std::string p = base_szs_path(t);
        if (p.empty()) return false;             // unknown target
        struct stat st;
        if (::stat(p.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
    }
    return true;
}

} // namespace thomaz
