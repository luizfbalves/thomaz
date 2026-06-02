#include "core/cheat_txt.hpp"
#include <sstream>

namespace thomaz::core {

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

// If `line` is a header, fill name/is_master and return true.
bool parse_header(const std::string& line, std::string& name, bool& is_master) {
    if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
        name = line.substr(1, line.size() - 2);
        is_master = false;
        return true;
    }
    if (line.size() >= 2 && line.front() == '{' && line.back() == '}') {
        name = line.substr(1, line.size() - 2);
        is_master = true;
        return true;
    }
    return false;
}

} // namespace

std::vector<Cheat> parse_txt(const std::string& content) {
    std::vector<Cheat> cheats;
    std::istringstream stream(content);
    std::string raw;
    bool have_current = false;

    while (std::getline(stream, raw)) {
        const std::string line = trim(raw);
        if (line.empty()) continue;

        std::string name;
        bool is_master = false;
        if (parse_header(line, name, is_master)) {
            Cheat c;
            c.name = name;
            c.is_master = is_master;
            cheats.push_back(std::move(c));
            have_current = true;
        } else if (have_current) {
            cheats.back().opcode_lines.push_back(line);
        }
        // lines before the first header are ignored
    }
    return cheats;
}

std::string serialize_txt(const std::vector<Cheat>& cheats, const std::set<std::string>& enabled) {
    std::string out;
    for (const Cheat& c : cheats) {
        const bool include = c.is_master || enabled.count(c.name) > 0;
        if (!include) continue;
        out += c.is_master ? ("{" + c.name + "}\n") : ("[" + c.name + "]\n");
        for (const std::string& line : c.opcode_lines) {
            out += line;
            out += "\n";
        }
        out += "\n"; // blank separator after each cheat
    }
    return out;
}

} // namespace thomaz::core
