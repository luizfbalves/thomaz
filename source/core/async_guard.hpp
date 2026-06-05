#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace thomaz::core {

// Run onSync and return true if the alive guard is non-null and still set.
// Returns false and drops onSync (without calling it) if the guard is null or
// has been cleared (e.g. by ~ThomazActivity).
//
// Pure, C++17-clean — safe to call from host tests and from brls::sync
// continuations alike.  Does not include or reference any UI framework headers.
inline bool run_if_alive(const std::shared_ptr<std::atomic<bool>>& alive,
                         const std::function<void()>& onSync)
{
    if (!alive || !alive->load())
        return false;
    onSync();
    return true;
}

} // namespace thomaz::core
