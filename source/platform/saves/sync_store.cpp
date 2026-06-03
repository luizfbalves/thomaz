#include "platform/saves/sync_store.hpp"
#include "core/saves/save_sync_state.hpp"
#include "platform/cheat_store.hpp" // read_text_file / write_text_file
#include <map>
#include <string>

namespace thomaz {

namespace {
std::string sync_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/save_sync.txt";
#else
    return "thomaz-cache/save_sync.txt";
#endif
}
} // namespace

int load_synced_revision(std::uint64_t title_id) {
    auto body = read_text_file(sync_file());
    if (!body) return 0;
    auto state = core::parse_sync_state(*body);
    return core::synced_revision(state, title_id);
}

void save_synced_revision(std::uint64_t title_id, int revision) {
    std::map<std::uint64_t, int> state;
    if (auto body = read_text_file(sync_file()))
        state = core::parse_sync_state(*body);
    state[title_id] = revision;
    write_text_file(sync_file(), core::serialize_sync_state(state));
}

} // namespace thomaz
