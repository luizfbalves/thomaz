#include "core/games/source_link.hpp"

#include <nlohmann/json.hpp>

namespace thomaz::core {

namespace {

using nlohmann::json;

const char* auth_type_to_string(SourceAuthType t) {
    switch (t) {
    case SourceAuthType::None:
        return "none";
    case SourceAuthType::BasicInUrl:
        return "basicInUrl";
    case SourceAuthType::Header:
        return "header";
    case SourceAuthType::Referrer:
        return "referrer";
    }
    return "none";
}

std::optional<SourceAuthType> auth_type_from_string(const std::string& s) {
    if (s == "none")
        return SourceAuthType::None;
    if (s == "basicInUrl")
        return SourceAuthType::BasicInUrl;
    if (s == "header")
        return SourceAuthType::Header;
    if (s == "referrer")
        return SourceAuthType::Referrer;
    return std::nullopt;
}

} // namespace

std::string serialize_source_link(const SourceConfig& cfg) {
    json j;
    j["label"]    = cfg.label;
    j["url"]      = cfg.url;
    j["authType"] = auth_type_to_string(cfg.authType);
    return j.dump();
}

std::optional<SourceConfig> parse_source_link(const std::string& body) {
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object())
        return std::nullopt;

    SourceConfig cfg;
    cfg.label = j.value("label", std::string{});
    cfg.url   = j.value("url", std::string{});
    const std::string authStr = j.value("authType", std::string{"none"});
    auto authType             = auth_type_from_string(authStr);
    if (!authType)
        return std::nullopt;
    cfg.authType = *authType;
    // authSecret is never carried in catalog/sync JSON from the API codec.
    cfg.authSecret.clear();
    return cfg;
}

} // namespace thomaz::core
