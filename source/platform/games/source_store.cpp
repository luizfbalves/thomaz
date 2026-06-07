// SD storage is plaintext-readable (accepted MVP limitation). Full E2E deferred.
#include "platform/games/source_store.hpp"

#include "platform/cheat_store.hpp"
#include "platform/games/index_fetch_util.hpp"

#include <nlohmann/json.hpp>

#ifdef __SWITCH__
#include <borealis.hpp>
#endif

namespace thomaz {

namespace {

using nlohmann::json;

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto a = s.find_first_not_of(ws);
    if (a == std::string::npos)
        return "";
    auto b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

std::optional<thomaz::core::SourceAuthType> auth_type_from_string(const std::string& s) {
    if (s == "none")
        return thomaz::core::SourceAuthType::None;
    if (s == "basicInUrl")
        return thomaz::core::SourceAuthType::BasicInUrl;
    if (s == "header")
        return thomaz::core::SourceAuthType::Header;
    if (s == "referrer")
        return thomaz::core::SourceAuthType::Referrer;
    return std::nullopt;
}

const char* auth_type_to_string(thomaz::core::SourceAuthType t) {
    switch (t) {
    case thomaz::core::SourceAuthType::None:
        return "none";
    case thomaz::core::SourceAuthType::BasicInUrl:
        return "basicInUrl";
    case thomaz::core::SourceAuthType::Header:
        return "header";
    case thomaz::core::SourceAuthType::Referrer:
        return "referrer";
    }
    return "none";
}

std::optional<thomaz::core::SourceConfig> parse_local_source(const json& j) {
    if (!j.is_object())
        return std::nullopt;
    thomaz::core::SourceConfig cfg;
    cfg.label = j.value("label", std::string{});
    cfg.url   = j.value("url", std::string{});
    const std::string authStr = j.value("authType", std::string{"none"});
    auto authType             = auth_type_from_string(authStr);
    if (!authType)
        return std::nullopt;
    cfg.authType   = *authType;
    cfg.authSecret = j.value("authSecret", std::string{});
    cfg.remoteId   = j.value("remoteId", std::string{});
    return cfg;
}

json serialize_local_source(const thomaz::core::SourceConfig& cfg) {
    json j;
    j["label"]      = cfg.label;
    j["url"]        = cfg.url;
    j["authType"]   = auth_type_to_string(cfg.authType);
    j["authSecret"] = cfg.authSecret;
    if (!cfg.remoteId.empty())
        j["remoteId"] = cfg.remoteId;
    return j;
}

} // namespace

std::string sources_config_path() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/sources.json";
#else
    return "thomaz-cache/sources.json";
#endif
}

std::vector<thomaz::core::SourceConfig> load_sources() {
    const auto body = read_text_file(sources_config_path());
    if (!body || trim(*body).empty())
        return {};

    json root = json::parse(*body, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded() || !root.is_array())
        return {};

    std::vector<thomaz::core::SourceConfig> out;
    out.reserve(root.size());
    for (const auto& item : root) {
        if (auto cfg = parse_local_source(item))
            out.push_back(std::move(*cfg));
    }
    return out;
}

bool save_sources(const std::vector<thomaz::core::SourceConfig>& sources) {
    json root = json::array();
    for (const auto& cfg : sources)
        root.push_back(serialize_local_source(cfg));

    const bool ok = write_text_file(sources_config_path(), root.dump(2));
#ifdef __SWITCH__
    if (ok && !sources.empty()) {
        brls::Logger::info("thomaz/sources: saved {} source(s) for host {}",
                           sources.size(),
                           redacted_host_from_url(sources.front().url));
    } else if (ok) {
        brls::Logger::info("thomaz/sources: saved empty source list");
    }
#endif
    return ok;
}

} // namespace thomaz
