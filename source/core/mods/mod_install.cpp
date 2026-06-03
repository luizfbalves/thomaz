#include "core/mods/mod_install.hpp"

namespace thomaz::core {

bool is_safe_archive_path(const std::string& path) {
    if (path.empty() || path.front() == '/')
        return false;
    std::size_t start = 0;
    for (;;) {
        std::size_t slash = path.find('/', start);
        std::string seg = (slash == std::string::npos)
                              ? path.substr(start)
                              : path.substr(start, slash - start);
        if (seg == "..")
            return false;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return true;
}

namespace {

// Returns the prefix up to and including the "romfs/" segment's parent, i.e.
// everything before "romfs/". Returns false in `found` if no romfs segment.
std::string romfs_strip_prefix(const std::string& path, bool& found) {
    // Look for a path segment exactly equal to "romfs".
    std::size_t start = 0;
    while (start < path.size()) {
        std::size_t slash = path.find('/', start);
        if (slash == std::string::npos)
            break; // last segment is a file name, never the romfs dir
        std::string seg = path.substr(start, slash - start);
        if (seg == "romfs") {
            found = true;
            return path.substr(0, start); // prefix before "romfs/"
        }
        start = slash + 1;
    }
    found = false;
    return {};
}

} // namespace

InstallPlan plan_install(const std::vector<ArchiveEntry>& entries) {
    InstallPlan plan;
    if (entries.empty()) {
        plan.error = InstallError::Empty;
        return plan;
    }

    bool any_romfs = false;
    bool prefix_set = false;
    std::string prefix;

    for (const ArchiveEntry& e : entries) {
        if (!is_safe_archive_path(e.path)) {
            plan.error = InstallError::UnsafePath;
            return plan;
        }
        bool found = false;
        std::string p = romfs_strip_prefix(e.path, found);
        if (!found)
            continue; // entries outside any romfs/ are ignored for the prefix
        any_romfs = true;
        if (!prefix_set) {
            prefix = p;
            prefix_set = true;
        } else if (p != prefix) {
            plan.error = InstallError::Ambiguous;
            return plan;
        }
    }

    if (!any_romfs) {
        plan.error = InstallError::NotRomfs;
        return plan;
    }

    plan.error = InstallError::None;
    plan.strip_prefix = prefix;
    return plan;
}

} // namespace thomaz::core
