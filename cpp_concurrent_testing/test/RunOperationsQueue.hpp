#pragma once

#include "../utils/QueueADT.hpp"
#include "RunController.hpp"
#include <atomic>
#include <vector>
#include <random>
#include <thread>

class RunOperationsQueue {
public:
    static void sanityRun(QueueADT* queue, int threadId, int keyRange, std::vector<int>& results,
                          std::atomic<int>& totalEnqueued, std::atomic<int>& totalDequeued) {
        while (!RunController::startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        int ops = 0;
        std::mt19937 randKey(threadId + std::random_device{}());
        std::uniform_int_distribution<int> keyDist(0, keyRange - 1);

        while (!RunController::stopFlag.load(std::memory_order_acquire)) {
            int val = keyDist(randKey);
            queue->enqueue(val);
            totalEnqueued.fetch_add(1, std::memory_order_relaxed);
            ops++;

            int got = queue->dequeue();
            if (got != -1) {
                totalDequeued.fetch_add(1, std::memory_order_relaxed);
                ops++;
            }
            RunController::globalOps.fetch_add(2, std::memory_order_relaxed);
        }
        results[threadId] = ops;
    }

    static void benchMarkRun(QueueADT* queue, int threadId, int keyRange, std::vector<int>& results) {
        while (!RunController::startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        int ops = 0;
        std::mt19937 randKey(threadId + 100 + std::random_device{}());
        std::uniform_int_distribution<int> keyDist(0, keyRange - 1);

        while (!RunController::stopFlag.load(std::memory_order_acquire)) {
            queue->enqueue(keyDist(randKey));
            queue->dequeue();
            ops += 2;
            RunController::globalOps.fetch_add(2, std::memory_order_relaxed);
        }
        results[threadId] = ops;
    }
};
