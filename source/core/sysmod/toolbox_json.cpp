#include "core/sysmod/toolbox_json.hpp"

#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

ToolboxInfo parse_toolbox(const std::string& raw) {
    ToolboxInfo info;
    json doc = json::parse(raw, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object())
        return info; // valid stays false

    info.valid = true;
    if (doc.contains("name") && doc["name"].is_string())
        info.name = doc["name"].get<std::string>();
    if (doc.contains("requires_reboot") && doc["requires_reboot"].is_boolean())
        info.requires_reboot = doc["requires_reboot"].get<bool>();
    return info;
}

} // namespace thomaz::core
