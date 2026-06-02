#include "TestConfig.hpp"
#include "RunOperationsStack.hpp"
#include "RunController.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>
#include <atomic>
#include <climits>
#include <functional>

#include "StructureFactory.hpp"

/**
 * Stack Consistency Test — mirrors Java StackConsistencyTest.
 *
 * Phase 1: Concurrent conservation check
 *   Threads push/pop concurrently with 2:1 push:pop ratio.
 *     pushed >= popped  AND  pushed <= popped + remaining + margin
 *
 * Phase 2: LIFO ordering check
 *   Each thread pushes values from a global orderCounter.
 *   After draining, remaining items should have been pushed EARLIER (lower tags)
 *   while popped items were pushed LATER (higher tags, popped via LIFO).
 *   Check: min(remaining) < max(popped) + tolerance
 */
int main(int argc, char** argv) {
    TestConfig config;
    if (!config.parse(argc, argv)) return 1;

    auto stack = createStack(config.algo);
    if (!stack) {
        std::cerr << "Algorithm " << config.algo << " not imp.\n";
        return 1;
    }

    std::cout << "Running Consistency Test for Stack: " << config.algo << "\n";

    // ── Phase 1 + 2: Concurrent push/pop with order tracking ────────────
    std::vector<int> results(config.numThreads, 0);
    std::vector<std::thread> threads;
    std::atomic<int> totalPushed{0}, totalPopped{0};
    std::atomic<long> orderCounter{0};
    // Per-thread popped values for LIFO check
    std::vector<std::vector<int>> allPopped(config.numThreads);

    RunController::reset();

    for (int i = 0; i < config.numThreads; i++) {
        threads.emplace_back([&, i]() {
            while (!RunController::startFlag.load(std::memory_order_acquire))
                std::this_thread::yield();

            int ops = 0;
            while (!RunController::stopFlag.load(std::memory_order_acquire)) {
                // Push two items (2:1 push:pop ratio)
                int val1 = (int)orderCounter.fetch_add(1, std::memory_order_relaxed);
                stack->push(val1);
                totalPushed.fetch_add(1, std::memory_order_relaxed);
                ops++;

                int val2 = (int)orderCounter.fetch_add(1, std::memory_order_relaxed);
                stack->push(val2);
                totalPushed.fetch_add(1, std::memory_order_relaxed);
                ops++;

                // Pop one item
                int got = stack->pop();
                if (got != -1) {
                    totalPopped.fetch_add(1, std::memory_order_relaxed);
                    allPopped[i].push_back(got);
                    ops++;
                }
                RunController::globalOps.fetch_add(3, std::memory_order_relaxed);
            }
            results[i] = ops;
        });
    }

    RunController::startFlag.store(true);
    std::this_thread::sleep_for(std::chrono::seconds(config.duration));
    RunController::stopFlag.store(true);

    for (auto& t : threads) t.join();

    // Drain remaining items and collect order tags
    std::vector<int> remainingOrders;
    while (!stack->isEmpty()) {
        int v = stack->pop();
        if (v == -1) break;
        remainingOrders.push_back(v);
    }
    int remaining = (int)remainingOrders.size();
    int pushed = totalPushed.load();
    int popped = totalPopped.load();

    // Check 1: Conservation
    bool conservationOk = (pushed >= popped) && (pushed <= popped + remaining + config.numThreads * 2);

    // Check 2: LIFO Ordering with tolerance for concurrent push races
    // In a LIFO structure, remaining items were pushed EARLIER (lower tags)
    // while popped items were pushed LATER (higher tags, popped via LIFO).
    int maxPopped = INT_MIN;
    bool hasPopped = false;
    for (auto& tp : allPopped) {
        for (int val : tp) {
            if (val > maxPopped) maxPopped = val;
            hasPopped = true;
        }
    }

    int minRemaining = INT_MAX;
    bool hasRemaining = false;
    for (int val : remainingOrders) {
        if (val < minRemaining) minRemaining = val;
        hasRemaining = true;
    }

    int tolerance = config.numThreads * 2;
    bool lifoOk = !hasPopped || !hasRemaining || (minRemaining < maxPopped + tolerance);
    bool passed = conservationOk && lifoOk;

    if (passed) {
        std::cout << "Pushed: " << pushed << ", Popped: " << popped << ", Remaining: " << remaining << "\n";
        if (hasPopped && hasRemaining)
            std::cout << "LIFO check: maxPopped=" << maxPopped << " minRemaining=" << minRemaining
                      << " tolerance=" << tolerance << " OK\n";
        std::cout << "Sanity Test Passed\n";
        return 0;
    } else {
        if (!conservationOk)
            std::cout << "Stack conservation failed: pushed=" << pushed
                      << " popped=" << popped << " remaining=" << remaining << "\n";
        if (!lifoOk)
            std::cout << "Stack LIFO ordering failed: maxPopped=" << maxPopped
                      << " minRemaining=" << minRemaining << " tolerance=" << tolerance << "\n";
        std::cout << "Sanity Test Failed\n";
        return 1;
    }
}
