#include "platform/themes/theme_install.hpp"
#include "platform/themes/cfw_paths.hpp"
#include "platform/themes/theme_paths.hpp"
#include "platform/themes/theme_download.hpp"   // nxtheme_filename
#include "platform/themes/active_theme_store.hpp"
#include "apply_facade.hpp"

#include <fstream>
#include <sys/stat.h>
#include <cstdio>

namespace thomaz {

namespace {

std::vector<std::string> detail_targets(const thomaz::core::ThemeDetail& d) {
    std::vector<std::string> ts;
    for (const auto& p : d.parts)
        if (!p.target.empty()) ts.push_back(p.target);
    return ts;
}

bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool write_file(const std::string& path, const std::vector<unsigned char>& data) {
    std::ofstream o(path, std::ios::binary | std::ios::trunc);
    if (!o) return false;
    if (!data.empty()) o.write((const char*)data.data(), (std::streamsize)data.size());
    return (bool)o;
}

// mkdir -p for the parent dirs of a file path (POSIX, FAT-safe).
void ensure_parent_dirs(const std::string& file) {
    std::string acc;
    for (size_t i = 0; i < file.size(); ++i) {
        acc.push_back(file[i]);
        if (file[i] == '/' && acc.size() > 1) ::mkdir(acc.c_str(), 0777);
    }
}

} // namespace

bool base_layouts_available(const thomaz::core::ThemeDetail& detail) {
    return base_present_for(detail_targets(detail));
}

InstallResult install_theme(const thomaz::core::ThemeDetail& detail) {
    InstallResult res;
    auto targets = detail_targets(detail);
    if (targets.empty()) { res.error = "theme has no installable parts"; return res; }
    if (!base_present_for(targets)) { res.error = "base layouts missing"; return res; }

    std::string folder = theme_folder(detail.entry);
    std::vector<std::string> written;        // for rollback
    std::vector<std::string> flags_written;  // flag files written, also rolled back on failure
    std::vector<std::string> applied_targets;

    auto rollback = [&]() {
        for (const auto& w : written)       ::remove(w.c_str());
        for (const auto& f : flags_written) ::remove(f.c_str());
    };

    int index = 0;
    for (const auto& part : detail.parts) {
        const int i = index++;
        if (part.target.empty()) continue;

        std::string nx_path  = folder + "/" + nxtheme_filename(part, i);
        std::string base     = base_szs_path(part.target);
        std::string out_path = output_szs_path(part.target);

        std::vector<unsigned char> nx_bytes, base_bytes;
        if (!read_file(nx_path, nx_bytes)) {
            rollback();
            res.error = "missing downloaded file: " + nx_path;
            return res;
        }
        if (!read_file(base, base_bytes)) {
            rollback();
            res.error = "missing base layout: " + base;
            return res;
        }

        switchthemes::ApplyOutput ao =
            switchthemes::apply_nxtheme(base_bytes, nx_bytes);
        for (const auto& w : ao.warnings) res.warnings.push_back(part.target + ": " + w);
        if (!ao.ok) {
            rollback();
            res.error = "engine: " + ao.error;
            return res;
        }

        ensure_parent_dirs(out_path);
        if (!write_file(out_path, ao.szs)) {
            rollback();
            res.error = "could not write " + out_path;
            return res;
        }
        written.push_back(out_path);
        applied_targets.push_back(part.target);

        // Legacy FS-MITM marker (harmless on modern Atmosphère).
        auto m = target_map(part.target);
        if (m) {
            std::string flag = layeredfs_root() + "/" + m->title_id + "/fsmitm.flag";
            ensure_parent_dirs(flag);
            std::ofstream(flag).put('\0');
            flags_written.push_back(flag);
        }
    }

    ActiveTheme at;
    at.hex_id  = detail.entry.hex_id;
    at.name    = detail.entry.name;
    at.author  = detail.entry.author;
    at.targets = applied_targets;
    set_active_theme(at);

    res.ok = true;
    return res;
}

InstallResult remove_active_theme() {
    InstallResult res;
    auto active = get_active_theme();
    if (!active) { res.ok = true; return res; }   // nothing to remove

    for (const auto& t : active->targets) {
        std::string out = output_szs_path(t);
        if (!out.empty()) ::remove(out.c_str());
    }
    clear_active_theme();
    res.ok = true;
    return res;
}

} // namespace thomaz
