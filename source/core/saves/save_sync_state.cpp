#include "core/saves/save_sync_state.hpp"
#include <climits>
#include <cstdio>
#include <sstream>

namespace thomaz::core {

std::map<std::uint64_t, int> parse_sync_state(const std::string& body) {
    std::map<std::uint64_t, int> state;
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        unsigned long long id = 0;
        long long rev = 0;
        // Expect "<hex> <int>"; require both fields and a revision that fits int.
        if (std::sscanf(line.c_str(), "%llx %lld", &id, &rev) == 2 &&
            rev >= 0 && rev <= INT_MAX)
            state[(std::uint64_t)id] = (int)rev;
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
