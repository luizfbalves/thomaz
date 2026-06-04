#include "platform/themes/active_theme_store.hpp"
#include "platform/themes/theme_paths.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sys/stat.h>

namespace thomaz {

namespace {
std::string active_path() { return themes_root() + "/.thomaz_active.json"; }
} // namespace

std::optional<ActiveTheme> get_active_theme() {
    std::ifstream in(active_path());
    if (!in) return std::nullopt;
    auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;

    ActiveTheme t;
    t.hex_id = j.value("hex_id", std::string());
    t.name   = j.value("name", std::string());
    t.author = j.value("author", std::string());
    if (j.contains("targets") && j["targets"].is_array()) {
        for (const auto& e : j["targets"])
            if (e.is_string()) t.targets.push_back(e.get<std::string>());
    }
    if (t.hex_id.empty()) return std::nullopt;
    return t;
}

void set_active_theme(const ActiveTheme& t) {
    ::mkdir(themes_root().c_str(), 0777);  // best-effort
    nlohmann::json j;
    j["hex_id"]  = t.hex_id;
    j["name"]    = t.name;
    j["author"]  = t.author;
    j["targets"] = t.targets;
    std::ofstream out(active_path(), std::ios::trunc);
    out << j.dump(2);
}

void clear_active_theme() {
    ::remove(active_path().c_str());
}

bool is_active_theme(const thomaz::core::ThemeEntry& entry) {
    auto a = get_active_theme();
    return a && !a->hex_id.empty() && a->hex_id == entry.hex_id;
}

} // namespace thomaz
