#pragma once

#include <cstdint>

namespace thomaz {

struct StorageInfo {
    std::uint64_t total_bytes = 0;
    std::uint64_t free_bytes  = 0;
};

struct SystemStatus {
    StorageInfo sd;
    StorageInfo nand;
    bool wifi_connected = false;
    int  wifi_strength  = 0;  // 0–3
};

// Query current system status (storage + WiFi).
// On non-Switch platforms returns a zeroed SystemStatus{}.
SystemStatus query_system_status();

} // namespace thomaz
