#include "platform/mods/archive_extractor.hpp"
#include "core/mods/mod_install.hpp" // is_safe_archive_path
#include "platform/fs_util.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <sys/stat.h>

namespace thomaz {

namespace {

struct ArchiveCloser {
    struct archive* a;
    ~ArchiveCloser() {
        if (a) {
            archive_read_close(a);
            archive_read_free(a);
        }
    }
};

struct archive* open_archive(const std::string& path) {
    struct archive* a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    if (archive_read_open_filename(a, path.c_str(), 10240) != ARCHIVE_OK) {
        archive_read_free(a);
        return nullptr;
    }
    return a;
}

} // namespace

std::vector<core::ArchiveEntry> list_archive_entries(const std::string& archive_path) {
    std::vector<core::ArchiveEntry> out;
    struct archive* a = open_archive(archive_path);
    if (!a)
        return out;
    ArchiveCloser closer{a};

    struct archive_entry* entry;
    int hr;
    while ((hr = archive_read_next_header(a, &entry)) == ARCHIVE_OK || hr == ARCHIVE_WARN) {
        const char* name = archive_entry_pathname(entry);
        if (!name)
            continue;
        bool is_dir = archive_entry_filetype(entry) == AE_IFDIR;
        out.push_back(core::ArchiveEntry{std::string(name), is_dir});
    }
    return out;
}

ExtractResult extract_archive(const std::string& archive_path,
                              const std::string& dest_dir,
                              const std::string& strip_prefix,
                              const std::function<void(int, int)>& progress) {
    ExtractResult result;

    int total = static_cast<int>(list_archive_entries(archive_path).size());

    struct archive* a = open_archive(archive_path);
    if (!a) {
        result.error = "cannot open archive";
        return result;
    }
    ArchiveCloser closer{a};

    ::mkdir(dest_dir.c_str(), 0777);

    struct archive_entry* entry;
    int seen = 0;
    int hr;
    while ((hr = archive_read_next_header(a, &entry)) == ARCHIVE_OK || hr == ARCHIVE_WARN) {
        ++seen;
        if (progress)
            progress(seen, total);

        const char* raw = archive_entry_pathname(entry);
        if (!raw)
            continue;
        std::string name = raw;

        // Apply the strip prefix; skip entries outside it.
        if (!strip_prefix.empty()) {
            if (name.rfind(strip_prefix, 0) != 0)
                continue;
            name = name.substr(strip_prefix.size());
        }
        if (name.empty())
            continue;
        if (!core::is_safe_archive_path(name))
            continue; // zip-slip guard

        std::string out_path = dest_dir + "/" + name;

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            ensure_parent_dirs(out_path + "/");
            ::mkdir(out_path.c_str(), 0777);
            continue;
        }

        ensure_parent_dirs(out_path);
        std::FILE* out = std::fopen(out_path.c_str(), "wb");
        if (!out) {
            result.error = "cannot write " + out_path;
            return result;
        }
        const void* buff;
        std::size_t size;
        la_int64_t offset;
        int r;
        bool wrote_ok = true;
        while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK ||
               r == ARCHIVE_WARN) {
            if (std::fwrite(buff, 1, size, out) != size) {
                wrote_ok = false;
                break;
            }
        }
        std::fclose(out);
        if (!wrote_ok || r != ARCHIVE_EOF) {
            result.error = "read/write error on " + name;
            return result;
        }
        ++result.files_written;
    }

    result.ok = true;
    return result;
}

} // namespace thomaz
