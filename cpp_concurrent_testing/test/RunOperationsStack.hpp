#pragma once

#include "../utils/StackADT.hpp"
#include "RunController.hpp"
#include <atomic>
#include <vector>
#include <random>
#include <thread>

class RunOperationsStack {
public:
    static void sanityRun(StackADT* stack, int threadId, int keyRange, std::vector<int>& results,
                          std::atomic<int>& totalPushed, std::atomic<int>& totalPopped) {
        while (!RunController::startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        int ops = 0;
        std::mt19937 randKey(threadId + std::random_device{}());
        std::uniform_int_distribution<int> keyDist(0, keyRange - 1);

        while (!RunController::stopFlag.load(std::memory_order_acquire)) {
            int val = keyDist(randKey);
            stack->push(val);
            totalPushed.fetch_add(1, std::memory_order_relaxed);
            ops++;

            int got = stack->pop();
            if (got != -1) {
                totalPopped.fetch_add(1, std::memory_order_relaxed);
                ops++;
            }
            RunController::globalOps.fetch_add(2, std::memory_order_relaxed);
        }
        results[threadId] = ops;
    }

    static void benchMarkRun(StackADT* stack, int threadId, int keyRange, std::vector<int>& results) {
        while (!RunController::startFlag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        int ops = 0;
        std::mt19937 randKey(threadId + 100 + std::random_device{}());
        std::uniform_int_distribution<int> keyDist(0, keyRange - 1);

        while (!RunController::stopFlag.load(std::memory_order_acquire)) {
            stack->push(keyDist(randKey));
            stack->pop();
            ops += 2;
            RunController::globalOps.fetch_add(2, std::memory_order_relaxed);
        }
        results[threadId] = ops;
    }
};
