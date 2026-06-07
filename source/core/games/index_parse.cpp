#include "core/games/index_parse.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

namespace {

using nlohmann::json;

void apply_name_override(IndexFile& e) {
    const auto hash = e.url.find('#');
    if (hash == std::string::npos || hash + 1 >= e.url.size())
        return;
    e.nameOverride = e.url.substr(hash + 1);
}

IndexFile parse_file_entry(const json& f) {
    IndexFile e;
    if (f.is_string()) {
        e.url = f.get<std::string>();
    } else if (f.is_object()) {
        e.url  = f.value("url", std::string{});
        e.size = f.value("size", 0ull);
    }
    apply_name_override(e);
    return e;
}

} // namespace

ParsedIndex parse_index(const std::string& body) {
    ParsedIndex out;
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object())
        return out;

    if (auto it = j.find("files"); it != j.end() && it->is_array()) {
        for (const auto& f : *it) {
            IndexFile e = parse_file_entry(f);
            if (!e.url.empty())
                out.files.push_back(std::move(e));
        }
    }

    if (auto it = j.find("directories"); it != j.end() && it->is_array()) {
        for (const auto& d : *it) {
            if (d.is_string())
                out.directories.push_back(d.get<std::string>());
        }
    }

    if (j.contains("success") && j["success"].is_string())
        out.motd = j["success"].get<std::string>();
    else if (j.contains("error") && j["error"].is_string())
        out.motd = j["error"].get<std::string>();

    out.ok = true;
    return out;
}

} // namespace thomaz::core
