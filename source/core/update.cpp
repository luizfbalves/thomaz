#include "core/update.hpp"
#include <nlohmann/json.hpp>
#include <vector>

namespace thomaz::core {

using nlohmann::json;

std::string github_latest_release_url() {
    return "https://api.github.com/repos/luizfbalves/thomaz/releases/latest";
}

ReleaseInfo parse_latest_release(const std::string& github_json,
                                 const std::string& asset_name) {
    ReleaseInfo r;
    json j = json::parse(github_json, nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return r;

    if (j.contains("tag_name") && j["tag_name"].is_string())
        r.tag = j["tag_name"].get<std::string>();
    if (j.contains("body") && j["body"].is_string())
        r.notes = j["body"].get<std::string>();

    if (j.contains("assets") && j["assets"].is_array()) {
        for (const auto& a : j["assets"]) {
            if (!a.is_object()) continue;
            if (a.value("name", std::string()) == asset_name &&
                a.contains("browser_download_url") &&
                a["browser_download_url"].is_string()) {
                r.nro_url = a["browser_download_url"].get<std::string>();
                break;
            }
        }
    }

    r.valid = !r.tag.empty() && !r.nro_url.empty();
    return r;
}

namespace {

// Split a version string into numeric components, ignoring a leading 'v'/'V'
// and any non-numeric prefix; non-numeric junk stops parsing.
std::vector<long> components(const std::string& v) {
    std::vector<long> out;
    std::size_t i = 0;
    while (i < v.size() && (v[i] < '0' || v[i] > '9'))
        ++i; // skip leading 'v', spaces, etc.
    long cur = 0;
    bool have = false;
    for (; i < v.size(); ++i) {
        char c = v[i];
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            have = true;
        } else if (c == '.') {
            out.push_back(have ? cur : 0);
            cur = 0;
            have = false;
        } else {
            break; // stop at suffixes like "-beta"
        }
    }
    if (have) out.push_back(cur);
    return out;
}

} // namespace

bool is_newer_version(const std::string& latest, const std::string& current) {
    std::vector<long> a = components(latest);
    std::vector<long> b = components(current);
    std::size_t n = a.size() > b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) {
        long ai = i < a.size() ? a[i] : 0;
        long bi = i < b.size() ? b[i] : 0;
        if (ai != bi) return ai > bi;
    }
    return false; // equal
}

} // namespace thomaz::core
