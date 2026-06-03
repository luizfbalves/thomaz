#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

struct CloudSlotMeta {
    bool         exists    = false;
    int          revision  = 0;
    std::string  label;
    std::int64_t updatedAt = 0;
};

struct CloudSlotData {
    CloudSlotMeta             meta;
    std::vector<std::uint8_t> data;
};

// 200 -> meta with exists=true; 404 -> meta with exists=false; else nullopt.
std::optional<CloudSlotMeta> parse_slot_meta(const std::string& body, long status);

// 200 -> meta + decoded blob; 404 -> meta exists=false, empty data; else nullopt.
std::optional<CloudSlotData> parse_slot_data(const std::string& body, long status);

// New revision from a PUT response body, or nullopt. The caller must only call
// this after confirming a 2xx status (it does not inspect the status itself).
std::optional<int> parse_push_revision(const std::string& body);

// "error" string from the body, or "http_<status>" if absent/unparseable.
std::string parse_error_message(const std::string& body, long status);

// Standard base64 decode; nullopt on invalid input.
std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& in);

} // namespace thomaz::core
