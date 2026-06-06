#include "platform/themes/theme_compat_cache.hpp"
#include "platform/themes/theme_paths.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sys/stat.h>

namespace thomaz {

namespace {

std::string cache_path() { return themes_root() + "/.thomaz_compat_cache.json"; }

bool fw_eq(const FwVersion& a, const FwVersion& b) {
    return a.major == b.major && a.minor == b.minor && a.micro == b.micro;
}

nlohmann::json fw_to_json(const FwVersion& fw) {
    return { { "major", fw.major }, { "minor", fw.minor }, { "micro", fw.micro } };
}

FwVersion fw_from_json(const nlohmann::json& j) {
    FwVersion fw;
    if (j.is_object()) {
        fw.major = j.value("major", 0u);
        fw.minor = j.value("minor", 0u);
        fw.micro = j.value("micro", 0u);
    }
    return fw;
}

nlohmann::json compat_to_json(const ThemeCompat& tc) {
    nlohmann::json parts = nlohmann::json::array();
    for (const auto& p : tc.parts) {
        parts.push_back({
            { "target", p.target },
            { "has_background", p.has_background },
            { "has_layout", p.has_layout },
            { "target_firmware", p.target_firmware },
            { "risk", static_cast<int>(p.risk) },
            { "detail", p.detail },
        });
    }
    return { { "overall", static_cast<int>(tc.overall) },
             { "dry_run_done", tc.dry_run_done },
             { "parts", parts } };
}

ThemeCompat compat_from_json(const nlohmann::json& j) {
    ThemeCompat tc;
    tc.overall      = static_cast<CompatRisk>(j.value("overall", 0));
    tc.dry_run_done = j.value("dry_run_done", false);
    if (j.contains("parts") && j["parts"].is_array()) {
        for (const auto& pj : j["parts"]) {
            PartCompat p;
            p.target          = pj.value("target", std::string());
            p.has_background  = pj.value("has_background", false);
            p.has_layout      = pj.value("has_layout", false);
            p.target_firmware = pj.value("target_firmware", 0);
            p.risk            = static_cast<CompatRisk>(pj.value("risk", 0));
            p.detail          = pj.value("detail", std::string());
            tc.parts.push_back(p);
        }
    }
    return tc;
}

} // namespace

std::optional<ThemeCompat> compat_cache_get(const std::string& hex_id, FwVersion fw) {
    std::ifstream in(cache_path());
    if (!in) return std::nullopt;
    auto j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return std::nullopt;
    if (!fw_eq(fw_from_json(j.value("fw", nlohmann::json::object())), fw)) return std::nullopt;
    if (!j.contains("entries") || !j["entries"].is_object()) return std::nullopt;
    const auto& entries = j["entries"];
    if (!entries.contains(hex_id)) return std::nullopt;
    return compat_from_json(entries[hex_id]);
}

void compat_cache_put(const std::string& hex_id, FwVersion fw, const ThemeCompat& tc) {
    ::mkdir(themes_root().c_str(), 0777); // best-effort

    nlohmann::json j;
    {
        std::ifstream in(cache_path());
        if (in) {
            auto parsed = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
            if (!parsed.is_discarded() && parsed.is_object()) j = parsed;
        }
    }

    // Reset the cache when the firmware tag is missing or differs (fw update).
    if (!j.contains("fw") || !fw_eq(fw_from_json(j["fw"]), fw)) {
        j = nlohmann::json::object();
        j["fw"]      = fw_to_json(fw);
        j["entries"] = nlohmann::json::object();
    }
    if (!j.contains("entries") || !j["entries"].is_object())
        j["entries"] = nlohmann::json::object();

    j["entries"][hex_id] = compat_to_json(tc);

    std::ofstream out(cache_path(), std::ios::trunc);
    out << j.dump(2);
}

} // namespace thomaz
