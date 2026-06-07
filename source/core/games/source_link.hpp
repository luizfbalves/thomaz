#pragma once

#include <optional>
#include <string>

namespace thomaz::core {

enum class SourceAuthType { None, BasicInUrl, Header, Referrer };

struct SourceConfig {
    std::string     label;
    std::string     url;
    SourceAuthType  authType   = SourceAuthType::None;
    std::string     authSecret;
    std::string     remoteId; // device-only cloud sync id; not in API codec
};

std::string serialize_source_link(const SourceConfig& cfg);
std::optional<SourceConfig> parse_source_link(const std::string& json);

} // namespace thomaz::core
