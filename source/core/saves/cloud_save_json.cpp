#include "core/saves/cloud_save_json.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

namespace {
json safe_parse(const std::string& body) {
    return json::parse(body, nullptr, /*allow_exceptions=*/false);
}

CloudSlotMeta read_meta(const json& slot) {
    CloudSlotMeta m;
    m.exists = true;
    if (slot.contains("revision") && slot["revision"].is_number_integer())
        m.revision = slot["revision"].get<int>();
    if (slot.contains("label") && slot["label"].is_string())
        m.label = slot["label"].get<std::string>();
    if (slot.contains("updatedAt") && slot["updatedAt"].is_number_integer())
        m.updatedAt = slot["updatedAt"].get<std::int64_t>();
    return m;
}
} // namespace

std::optional<CloudSlotMeta> parse_slot_meta(const std::string& body, long status) {
    if (status == 404) return CloudSlotMeta{}; // exists=false
    if (status < 200 || status >= 300) return std::nullopt;
    json j = safe_parse(body);
    if (!j.is_object() || !j.contains("slot") || !j["slot"].is_object())
        return std::nullopt;
    return read_meta(j["slot"]);
}

std::optional<CloudSlotData> parse_slot_data(const std::string& body, long status) {
    if (status == 404) return CloudSlotData{}; // meta.exists=false, empty data
    if (status < 200 || status >= 300) return std::nullopt;
    json j = safe_parse(body);
    if (!j.is_object() || !j.contains("slot") || !j["slot"].is_object())
        return std::nullopt;
    CloudSlotData d;
    d.meta = read_meta(j["slot"]);
    const json& slot = j["slot"];
    if (slot.contains("data") && slot["data"].is_string()) {
        auto decoded = base64_decode(slot["data"].get<std::string>());
        if (!decoded) return std::nullopt;
        d.data = std::move(*decoded);
    }
    return d;
}

std::optional<int> parse_push_revision(const std::string& body) {
    json j = safe_parse(body);
    if (j.is_object() && j.contains("slot") && j["slot"].is_object()) {
        const json& slot = j["slot"];
        if (slot.contains("revision") && slot["revision"].is_number_integer())
            return slot["revision"].get<int>();
    }
    return std::nullopt;
}

std::string parse_error_message(const std::string& body, long status) {
    json j = safe_parse(body);
    if (j.is_object() && j.contains("error") && j["error"].is_string())
        return j["error"].get<std::string>();
    return "http_" + std::to_string(status);
}

std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=') break;
        if (c == '\n' || c == '\r') continue;
        int v = val(c);
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((std::uint8_t)((buf >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace thomaz::core
