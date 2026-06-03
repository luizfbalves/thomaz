#include "core/saves/save_sync_state.hpp"
#include <cstdio>
#include <sstream>

namespace thomaz::core {

std::map<std::uint64_t, int> parse_sync_state(const std::string& body) {
    std::map<std::uint64_t, int> state;
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        unsigned long long id = 0;
        int rev = 0;
        // Expect exactly "<hex> <int>"; sscanf returns the count of matches.
        if (std::sscanf(line.c_str(), "%llx %d", &id, &rev) == 2)
            state[(std::uint64_t)id] = rev;
    }
    return state;
}

std::string serialize_sync_state(const std::map<std::uint64_t, int>& state) {
    std::string out;
    char buf[40];
    for (const auto& [id, rev] : state) {
        std::snprintf(buf, sizeof(buf), "%016llx %d\n",
                      (unsigned long long)id, rev);
        out += buf;
    }
    return out;
}

int synced_revision(const std::map<std::uint64_t, int>& state, std::uint64_t titleId) {
    auto it = state.find(titleId);
    return it == state.end() ? 0 : it->second;
}

} // namespace thomaz::core
