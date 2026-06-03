#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace thomaz::core {

// In-memory map of the last cloud revision we synced for each title.
// On-disk form: one line per title, "<titleId 16-hex> <revision>".

std::map<std::uint64_t, int> parse_sync_state(const std::string& body);
std::string serialize_sync_state(const std::map<std::uint64_t, int>& state);

// Revision last synced for `titleId`, or 0 if we have never synced it.
int synced_revision(const std::map<std::uint64_t, int>& state, std::uint64_t titleId);

} // namespace thomaz::core
