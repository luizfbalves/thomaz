#include "platform/atmosphere_config.hpp"
#include "platform/cheat_store.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace thomaz {
namespace {

constexpr const char* kIniPath = "/atmosphere/config/system_settings.ini";
constexpr const char* kSection = "atmosphere";
constexpr const char* kKey     = "dmnt_cheats_enabled_by_default";
constexpr const char* kDesired = "dmnt_cheats_enabled_by_default = u8!0x1";

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Does this trimmed line set kKey (possibly behind a ';'/'#' comment marker)?
// On a match, reports whether it was commented out and the raw value after '='.
bool is_key_line(const std::string& trimmedLine, bool& commented, std::string& value) {
    std::string l = trimmedLine;
    commented = false;
    while (!l.empty() && (l[0] == ';' || l[0] == '#')) {
        commented = true;
        l = trim(l.substr(1));
    }
    const std::string key = kKey;
    if (lower(l).compare(0, key.size(), key) != 0)
        return false;
    std::string rest = trim(l.substr(key.size()));
    if (rest.empty() || rest[0] != '=')
        return false; // e.g. a longer key that merely starts with kKey
    value = trim(rest.substr(1));
    return true;
}

// Truthiness of an Atmosphère typed value like "u8!0x1" / "0x0" / "1".
bool value_is_on(const std::string& value) {
    std::string v = value;
    if (auto bang = v.find('!'); bang != std::string::npos)
        v = v.substr(bang + 1);
    v = lower(trim(v));
    return !(v.empty() || v == "0x0" || v == "0" || v == "false");
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur.push_back(c);
        }
    }
    lines.push_back(cur); // trailing segment (keeps a final newline on rejoin)
    return lines;
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size())
            out += "\n";
    }
    return out;
}

} // namespace

CheatsDefaultResult ensure_cheats_enabled_by_default() {
    std::optional<std::string> existing = read_text_file(kIniPath);

    // No config file yet: create a minimal one with just our setting.
    if (!existing) {
        std::string body = std::string("[") + kSection + "]\n" + kDesired + "\n";
        return write_text_file(kIniPath, body) ? CheatsDefaultResult::Enabled
                                               : CheatsDefaultResult::Failed;
    }

    std::vector<std::string> lines = split_lines(*existing);

    std::string curSection;
    int atmoHeaderIdx = -1; // index of the "[atmosphere]" header line
    int keyLineIdx    = -1; // index of an existing kKey line within [atmosphere]
    bool keyOn        = false;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string t = trim(lines[i]);
        if (t.size() >= 2 && t.front() == '[' && t.back() == ']') {
            curSection = lower(trim(t.substr(1, t.size() - 2)));
            if (curSection == kSection && atmoHeaderIdx < 0)
                atmoHeaderIdx = (int)i;
            continue;
        }
        if (curSection == kSection) {
            bool commented = false;
            std::string val;
            if (is_key_line(t, commented, val)) {
                keyLineIdx = (int)i;
                keyOn      = !commented && value_is_on(val);
                break; // first occurrence governs what Atmosphère uses
            }
        }
    }

    if (keyLineIdx >= 0 && keyOn)
        return CheatsDefaultResult::AlreadyEnabled;

    if (keyLineIdx >= 0) {
        // Replace the commented-out / off line in place.
        lines[keyLineIdx] = kDesired;
    } else if (atmoHeaderIdx >= 0) {
        // Section exists but lacks the key — insert right after the header.
        lines.insert(lines.begin() + atmoHeaderIdx + 1, kDesired);
    } else {
        // No [atmosphere] section at all — append one.
        if (!lines.empty() && !trim(lines.back()).empty())
            lines.push_back("");
        lines.push_back(std::string("[") + kSection + "]");
        lines.push_back(kDesired);
    }

    return write_text_file(kIniPath, join_lines(lines)) ? CheatsDefaultResult::Enabled
                                                        : CheatsDefaultResult::Failed;
}

} // namespace thomaz
