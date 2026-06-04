#include "core/sysmod/sysmod_scan.hpp"
#include "core/sysmod/toolbox_json.hpp"

#include <algorithm>
#include <cctype>

namespace thomaz::core {

namespace {
std::string lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // namespace

std::vector<Sysmodule> build_sysmodule_list(const std::vector<RawSysmoduleEntry>& entries) {
    std::vector<Sysmodule> out;
    for (const RawSysmoduleEntry& e : entries) {
        if (!e.has_exefs)
            continue; // a game mod's romfs folder, not a sysmodule

        Sysmodule s;
        s.program_id = e.program_id;
        s.enabled    = e.has_boot2_flag;

        ToolboxInfo t = parse_toolbox(e.toolbox_json);
        s.has_metadata    = t.valid;
        s.requires_reboot = t.requires_reboot;
        s.name = (!t.name.empty()) ? t.name : e.program_id;

        out.push_back(std::move(s));
    }

    std::sort(out.begin(), out.end(), [](const Sysmodule& a, const Sysmodule& b) {
        return lower(a.name) < lower(b.name);
    });
    return out;
}

} // namespace thomaz::core
