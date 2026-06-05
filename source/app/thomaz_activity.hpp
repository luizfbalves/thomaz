#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include <borealis.hpp>
#include <borealis/core/thread.hpp>

#include "core/async_guard.hpp"

namespace thomaz {

// Base class that every thomaz activity should inherit instead of brls::Activity
// directly.  Owns the two lifetime/cancellation guards used for async dispatch:
//
//   alive     — set to false by the destructor; runAsync drops its onSync
//               continuation when the guard is cleared (CONC-02).
//
//   cancelled — set to true by the destructor; Platform-layer curl transfers
//               check this flag in their XFERINFOFUNCTION to abort in-flight
//               requests when the activity is popped (CONC-03 wiring, Plan 04).
//
// Derived classes call runAsync(worker, onSync) instead of the hand-rolled
// brls::async / alive-capture idiom.  The worker runs on the Borealis async
// pool and must NOT touch `this`.  onSync runs on the UI thread only when the
// activity is still alive at the time brls::sync fires.
class ThomazActivity : public brls::Activity
{
  public:
    // Sets alive=false and cancelled=true so that in-flight pool tasks and
    // curl transfers bail as soon as they reach the next guard check.
    virtual ~ThomazActivity()
    {
        *alive     = false;
        *cancelled = true;
    }

  protected:
    // Both guards are shared_ptr so worker lambdas can capture them by value
    // and the flag object outlives the activity even when the pool thread runs
    // after the dtor has completed.
    std::shared_ptr<std::atomic<bool>> alive =
        std::make_shared<std::atomic<bool>>(true);

    std::shared_ptr<std::atomic<bool>> cancelled =
        std::make_shared<std::atomic<bool>>(false);

    // Expose cancelled to derived classes (and transitively to the platform
    // layer) so Plan 04 (CONC-03) can hand it to curl transfer calls.
    std::shared_ptr<std::atomic<bool>> cancelledFlag() const
    {
        return cancelled;
    }

    // Dispatch worker on the Borealis async pool and, when it finishes, post
    // onSync back to the UI thread — but only if the activity is still alive.
    //
    // The worker is captured by value (moved into the lambda) and must NOT
    // access `this`; copy any data it needs before calling runAsync.
    // onSync runs on the UI thread and may freely reference members via `this`.
    template <typename Worker, typename OnSync>
    void runAsync(Worker worker, OnSync onSync)
    {
        auto aliveCapture = this->alive; // capture shared_ptr by value
        brls::async([aliveCapture,
                     w = std::move(worker),
                     s = std::move(onSync)]() mutable {
            w(); // pool thread — must not touch the activity
            brls::sync([aliveCapture, s]() mutable {
                thomaz::core::run_if_alive(aliveCapture, s);
            });
        });
    }
};

} // namespace thomaz
