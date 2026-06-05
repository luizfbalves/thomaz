#include "platform/title_visibility_store.hpp"
#include "platform/cheat_store.hpp"
#include "platform/fs_util.hpp"
#include "core/title_filter.hpp"

#include <cstdio>
#include <sstream>
#include <string>

namespace thomaz {

namespace {

std::string visibility_file()
{
#ifdef __SWITCH__
    return "/switch/thomaz/config/title_visibility.txt";
#else
    return "thomaz-cache/title_visibility.txt";
#endif
}

// Parse a 0x-prefixed hex id string. Returns 0 on failure.
std::uint64_t parse_hex_id(const char* s)
{
    char* end = nullptr;
    std::uint64_t v = std::strtoull(s, &end, 16);
    if (!end || end == s) return 0;
    return v;
}

// Validate that the string after the prefix looks like a 16-digit hex id (with optional 0x prefix).
bool is_valid_hex_id(const std::string& s)
{
    // Accept "0x" + up to 16 hex digits OR bare 16 hex digits.
    const char* p = s.c_str();
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
        p += 2;
    size_t len = 0;
    for (; p[len]; ++len) {
        char c = p[len];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return len >= 1 && len <= 16;
}

} // namespace

void TitleVisibilityStore::load()
{
    auto doc = read_text_file(visibility_file());
    if (!doc)
        return; // file absent — use defaults

    force_hidden_.clear();
    force_shown_.clear();
    show_hidden_ = false;

    std::istringstream ss(*doc);
    std::string line;
    while (std::getline(ss, line)) {
        // Trim trailing \r for Windows-style line endings.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty())
            continue;

        if (line == "SHOW_HIDDEN:1") {
            show_hidden_ = true;
        } else if (line == "SHOW_HIDDEN:0") {
            show_hidden_ = false;
        } else if (line.size() > 2 && line[0] == 'H' && line[1] == ':') {
            // T-rcu-01: validate before inserting; skip malformed lines.
            std::string id_str = line.substr(2);
            if (is_valid_hex_id(id_str)) {
                std::uint64_t id = parse_hex_id(id_str.c_str());
                if (id != 0)
                    force_hidden_.insert(id);
            }
        } else if (line.size() > 2 && line[0] == 'S' && line[1] == ':') {
            std::string id_str = line.substr(2);
            if (is_valid_hex_id(id_str)) {
                std::uint64_t id = parse_hex_id(id_str.c_str());
                if (id != 0)
                    force_shown_.insert(id);
            }
        }
        // Unknown lines are ignored silently (T-rcu-01).
    }
}

void TitleVisibilityStore::save() const
{
    std::string path = visibility_file();
    ensure_parent_dirs(path);

    std::string body;
    char buf[32];

    for (std::uint64_t id : force_hidden_) {
        std::snprintf(buf, sizeof(buf), "H:0x%016llX\n",
                      static_cast<unsigned long long>(id));
        body += buf;
    }
    for (std::uint64_t id : force_shown_) {
        std::snprintf(buf, sizeof(buf), "S:0x%016llX\n",
                      static_cast<unsigned long long>(id));
        body += buf;
    }
    body += show_hidden_ ? "SHOW_HIDDEN:1\n" : "SHOW_HIDDEN:0\n";

    write_text_file(path, body);
}

void TitleVisibilityStore::toggle_title(const InstalledTitle& t)
{
    std::uint64_t id = t.title_id;

    if (force_shown_.count(id)) {
        // Was force_shown → move to force_hidden.
        force_shown_.erase(id);
        force_hidden_.insert(id);
    } else if (force_hidden_.count(id)) {
        // Was force_hidden → move to force_shown.
        force_hidden_.erase(id);
        force_shown_.insert(id);
    } else {
        // Default state: use heuristic to decide direction.
        // Pass empty sets so we get the raw classify() answer.
        if (core::effectively_hidden(t, {}, {})) {
            // Title is hidden by default → user wants to force show it.
            force_shown_.insert(id);
        } else {
            // Title is visible by default → user wants to force hide it.
            force_hidden_.insert(id);
        }
    }
}

} // namespace thomaz
