#include "TestConfig.hpp"
#include "RunOperationsStack.hpp"
#include "RunController.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>
#include <atomic>

#include "StructureFactory.hpp"

/**
 * Stack Non-Blocking (Lock-Freedom) Test — mirrors Java StackNonBlockingTest.
 *
 * The ConcurrentDataStructure is pre-instrumented by the Python victim_injector
 * with sleep(5s) after successful CAS in pop().
 * One thread becomes the victim and stalls; the remaining threads must still
 * make progress, proving lock-freedom.
 *
 * Progress is measured by RunController::globalOps. If ops increase
 * while the victim is stalled, lock-freedom is proven.
 *
 * Prints "Sanity Test Passed" or "Sanity Test Failed".
 */
int main(int argc, char** argv) {
    TestConfig config;
    if (!config.parse(argc, argv)) return 1;

    std::cout << "Stack NonBlocking Experiment: Algo:" << config.algo
              << ", Duration:" << config.duration << "s"
              << ", Threads:" << config.numThreads << ", Keys:" << config.keyspaceSize << "\n";

    auto stack = createStack(config.algo);
    if (!stack) {
        std::cerr << "Algorithm " << config.algo << " not implemented yet.\n";
        return 1;
    }

    // Pre-populate so pop() has something to consume
    for (int i = 0; i < config.keyspaceSize / 2; i++) {
        stack->push(i);
    }

    std::vector<int> results(config.numThreads, 0);
    std::vector<std::thread> threads;
    RunController::reset();

    for (int i = 0; i < config.numThreads; i++) {
        threads.emplace_back(RunOperationsStack::benchMarkRun, stack.get(), i,
                             config.keyspaceSize, std::ref(results));
    }

    RunController::startFlag.store(true, std::memory_order_release);

    // Sample globalOps at t=1s and t=duration to confirm progress
    std::this_thread::sleep_for(std::chrono::seconds(1));
    long opsBefore = RunController::globalOps.load(std::memory_order_acquire);
    std::this_thread::sleep_for(std::chrono::seconds(config.duration - 1));
    long opsAfter = RunController::globalOps.load(std::memory_order_acquire);

    RunController::stopFlag.store(true, std::memory_order_release);
    for (auto& t : threads) t.join();

    // Lock-freedom: other threads made progress while victim stalled
    bool passed = (opsAfter - opsBefore) >= (config.numThreads - 1) * 5;

    if (passed) {
        std::cout << "Sanity Test Passed\n";
    } else {
        std::cout << "[StackNonBlockingTest] Progress check failed: opsBefore="
                  << opsBefore << " opsAfter=" << opsAfter << "\n";
        std::cout << "Sanity Test Failed\n";
    }

    return passed ? 0 : 1;
}
