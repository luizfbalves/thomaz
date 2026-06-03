#pragma once
#include <cstdint>

namespace thomaz {

// Last cloud revision synced for a title, persisted on the SD (Switch) or the
// working dir (desktop). 0 if never synced. Backed by the save_sync_state codec.
int  load_synced_revision(std::uint64_t title_id);
void save_synced_revision(std::uint64_t title_id, int revision);

} // namespace thomaz
