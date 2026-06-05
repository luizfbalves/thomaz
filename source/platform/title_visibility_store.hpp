#pragma once
#include <cstdint>
#include <set>
#include "platform/title.hpp"
#include "core/title_filter.hpp"

namespace thomaz {

// Persists the per-title visibility overrides and the global show_hidden toggle.
// File format (one entry per line):
//   H:0x01234567890ABCDE   — title id in force_hidden
//   S:0x0100000000010000   — title id in force_shown
//   SHOW_HIDDEN:1          — global show_hidden flag
//
// All load errors (missing file, parse errors) are handled silently; the store
// defaults to empty sets and show_hidden=false.
class TitleVisibilityStore {
  public:
    TitleVisibilityStore() = default;

    // Load from the platform-specific config file. Silences all I/O errors.
    void load();

    // Persist the current state to the platform-specific config file.
    // Creates parent directories automatically.
    void save() const;

    bool show_hidden() const { return show_hidden_; }
    void toggle_show_hidden() { show_hidden_ = !show_hidden_; }

    const std::set<std::uint64_t>& force_hidden() const { return force_hidden_; }
    const std::set<std::uint64_t>& force_shown()  const { return force_shown_;  }

    // Toggle the per-title visibility override.
    // Cycle: default → forced → opposite_forced → default
    // If currently force_shown:  move to force_hidden.
    // If currently force_hidden: move to force_shown.
    // If neither: if the title is effectively hidden (by heuristic) → add to force_shown;
    //             otherwise → add to force_hidden.
    void toggle_title(const InstalledTitle& t);

  private:
    std::set<std::uint64_t> force_hidden_;
    std::set<std::uint64_t> force_shown_;
    bool show_hidden_ = false;
};

} // namespace thomaz
