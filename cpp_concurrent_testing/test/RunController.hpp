#pragma once

#include <atomic>

struct RunController {
    static inline std::atomic<bool> startFlag{false};
    static inline std::atomic<bool> stopFlag{false};
    static inline std::atomic<long> globalOps{0};

    static void reset() {
        startFlag.store(false);
        stopFlag.store(false);
        globalOps.store(0);
    }
};
