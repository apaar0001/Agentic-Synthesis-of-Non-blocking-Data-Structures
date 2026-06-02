#include "TestConfig.hpp"
#include "RunOperationsQueue.hpp"
#include "RunController.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>
#include <atomic>
#include <functional>

#include "StructureFactory.hpp"

/**
 * Queue Consistency Test — mirrors Java QueueConsistencyTest.
 *
 * Phase 1: Concurrent conservation check
 *   Threads enqueue/dequeue concurrently. Afterwards:
 *     enqueued >= dequeued  AND  enqueued <= dequeued + remaining + margin
 *
 * Phase 2: FIFO per-producer ordering check
 *   Each producer encodes values as (tid * OFFSET + seqNum).
 *   After all producers finish, a single-threaded drain verifies that
 *   for each producer, its items appear in strictly increasing seqNum order.
 */
int main(int argc, char** argv) {
    TestConfig config;
    if (!config.parse(argc, argv)) return 1;

    auto queue = createQueue(config.algo);
    if (!queue) {
        std::cerr << "Algorithm " << config.algo << " not imp.\n";
        return 1;
    }

    std::cout << "Running Consistency Test for Queue: " << config.algo << "\n";

    // ── Phase 1: Concurrent conservation check ──────────────────────────
    std::vector<int> results(config.numThreads, 0);
    std::vector<std::thread> threads;
    std::atomic<int> totalEnqueued{0}, totalDequeued{0};
    RunController::reset();

    for (int i = 0; i < config.numThreads; i++) {
        threads.emplace_back(RunOperationsQueue::sanityRun, queue.get(), i,
                             config.keyspaceSize, std::ref(results),
                             std::ref(totalEnqueued), std::ref(totalDequeued));
    }

    RunController::startFlag.store(true);
    std::this_thread::sleep_for(std::chrono::seconds(config.duration));
    RunController::stopFlag.store(true);

    for (auto& t : threads) t.join();

    // Drain remaining items
    int remaining = 0;
    while (!queue->isEmpty()) {
        int v = queue->dequeue();
        if (v == -1) break;
        remaining++;
    }

    int enq = totalEnqueued.load();
    int deq = totalDequeued.load();

    // Identity: enq == deq + remaining (within a small margin for concurrent dequeues)
    bool conservationOk = (enq >= deq) && (enq <= deq + remaining + config.numThreads * 2);

    // ── Phase 2: Per-Producer FIFO ordering check ───────────────────────
    const int OFFSET = 1000000;
    const int ITEMS_PER_PRODUCER = 500;
    const int NUM_PRODUCERS = config.numThreads;

    auto queue2 = createQueue(config.algo);
    if (!queue2) {
        std::cout << "Sanity Test Failed\n";
        return 1;
    }

    // Phase 2a: All producers enqueue concurrently
    std::vector<std::thread> producers;
    std::atomic<bool> goFlag{false};

    for (int t = 0; t < NUM_PRODUCERS; t++) {
        producers.emplace_back([&, t]() {
            while (!goFlag.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int s = 0; s < ITEMS_PER_PRODUCER; s++) {
                queue2->enqueue(t * OFFSET + s);
            }
        });
    }
    goFlag.store(true, std::memory_order_release);
    for (auto& t : producers) t.join();

    // Phase 2b: Single-threaded dequeue to get deterministic ordering
    std::vector<int> globalDequeueOrder;
    while (true) {
        int v = queue2->dequeue();
        if (v == -1) break;
        globalDequeueOrder.push_back(v);
    }

    // Phase 2c: Verify per-producer ordering
    bool fifoOk = true;
    std::string fifoDetail;
    for (int prod = 0; prod < NUM_PRODUCERS; prod++) {
        int lastSeq = -1;
        for (int val : globalDequeueOrder) {
            int origProducer = val / OFFSET;
            int seqNum = val % OFFSET;
            if (origProducer == prod) {
                if (seqNum <= lastSeq) {
                    fifoDetail = "producer " + std::to_string(prod) +
                                 ": seq " + std::to_string(seqNum) +
                                 " appeared after seq " + std::to_string(lastSeq);
                    fifoOk = false;
                    break;
                }
                lastSeq = seqNum;
            }
        }
        if (!fifoOk) break;
    }

    int totalExpected = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    int totalGot = (int)globalDequeueOrder.size();

    bool passed = conservationOk && fifoOk;

    if (passed) {
        std::cout << "Enqueued: " << enq << ", Dequeued: " << deq << ", Remaining: " << remaining << "\n";
        std::cout << "FIFO per-producer check: " << NUM_PRODUCERS << " producers x "
                  << ITEMS_PER_PRODUCER << " items, " << totalGot << "/" << totalExpected << " dequeued OK\n";
        std::cout << "Sanity Test Passed\n";
        return 0;
    } else {
        if (!conservationOk)
            std::cout << "Queue conservation failed: enqueued=" << enq
                      << " dequeued=" << deq << " remaining=" << remaining << "\n";
        if (!fifoOk)
            std::cout << "Queue FIFO ordering failed: " << fifoDetail << "\n";
        std::cout << "Sanity Test Failed\n";
        return 1;
    }
}
