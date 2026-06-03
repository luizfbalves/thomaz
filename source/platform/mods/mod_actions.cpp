#include "platform/mods/mod_actions.hpp"

#include "core/mods/mod_install.hpp"
#include "core/mods/mod_paths.hpp"
#include "platform/mods/archive_extractor.hpp"
#include "platform/mods/mod_store.hpp"

namespace thomaz {

using core::mod_staging_dir;
using core::mod_staging_title_dir;
using core::active_marker_path;
using core::sd_romfs_dir;

ModActionResult import_archive(std::uint64_t title_id, const std::string& mod_name,
                               const std::string& archive_path,
                               const std::function<void(int, int)>& progress) {
    ModActionResult res;

    std::vector<core::ArchiveEntry> entries = list_archive_entries(archive_path);
    core::InstallPlan plan = core::plan_install(entries);
    if (!plan.ok()) {
        res.error = "not a valid romfs mod archive";
        return res;
    }

    std::string dest = mod_staging_dir(title_id, mod_name);
    ExtractResult ex = extract_archive(archive_path, dest, plan.strip_prefix, progress);
    if (!ex.ok) {
        res.error = ex.error;
        return res;
    }
    res.ok = true;
    return res;
}

std::vector<core::StagedMod> installed_mods(std::uint64_t title_id) {
    std::string active = active_mod(title_id);
    std::vector<core::StagedMod> out;
    for (const std::string& name : list_subdirs(mod_staging_title_dir(title_id)))
        out.push_back(core::StagedMod{name, name == active});
    return out;
}

std::string active_mod(std::uint64_t title_id) {
    auto m = read_marker(active_marker_path(title_id));
    return m.value_or("");
}

ModActionResult enable_mod(std::uint64_t title_id, const std::string& mod_name) {
    ModActionResult res;

    // Disable whatever is currently applied so contents/<tid>/romfs is clean.
    ModActionResult dis = disable_mod(title_id);
    if (!dis.ok) {
        res.error = dis.error;
        return res;
    }

    std::string src = mod_staging_dir(title_id, mod_name) + "/romfs";
    std::string dst = sd_romfs_dir(title_id);
    std::string err;
    if (!copy_tree(src, dst, &err)) {
        res.error = err;
        return res;
    }
    if (!write_marker(active_marker_path(title_id), mod_name)) {
        res.error = "could not write active marker";
        return res;
    }
    res.ok = true;
    return res;
}

ModActionResult disable_mod(std::uint64_t title_id) {
    ModActionResult res;
    if (!remove_tree(sd_romfs_dir(title_id))) {
        res.error = "could not remove applied romfs";
        return res;
    }
    clear_marker(active_marker_path(title_id));
    res.ok = true;
    return res;
}

ModActionResult uninstall_mod(std::uint64_t title_id, const std::string& mod_name) {
    ModActionResult res;
    if (active_mod(title_id) == mod_name) {
        ModActionResult dis = disable_mod(title_id);
        if (!dis.ok)
            return dis;
    }
    if (!remove_tree(mod_staging_dir(title_id, mod_name))) {
        res.error = "could not remove staged mod";
        return res;
    }
    res.ok = true;
    return res;
}

} // namespace thomaz
