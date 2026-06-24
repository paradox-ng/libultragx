#pragma once

// Synchronous stand-in for the BS::thread_pool used by the resource/engine layer.
// The GameCube has a single core for game work; rather than spin up real threads,
// submitted tasks run inline on submit. A game that offloads loading to the pool
// (e.g. behind a loading screen) still completes - it just blocks while it loads.

#include <cstddef>
#include <utility>

namespace BS {

class thread_pool {
  public:
    explicit thread_pool(std::size_t = 0) {}

    template <typename F> void submit_task(F&& task) {
        std::forward<F>(task)();
    }
    template <typename F> void detach_task(F&& task) {
        std::forward<F>(task)();
    }
    template <typename F> void push_task(F&& task) {
        std::forward<F>(task)();
    }

    void wait() {}
    void wait_for_tasks() {}
    std::size_t get_thread_count() const { return 1; }
};

} // namespace BS
