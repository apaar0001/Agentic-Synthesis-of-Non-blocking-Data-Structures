#pragma once

#include "../utils/SetADT.hpp"
#include "../utils/Zipf.hpp"
#include "RunController.hpp"
#include <vector>
#include <random>
#include <thread>
#include <iostream>

class RunOperations {
public:
    static void benchMarkRun(SetADT* set, int threadId, int addPercent, int removePercent, 
                             int keyRange, std::vector<int>& results) {
        std::mt19937 randOp(threadId + std::random_device{}());
        std::mt19937 randKey(threadId + 100 + std::random_device{}());
        std::uniform_int_distribution<int> opDist(0, 99);
        std::uniform_int_distribution<int> keyDist(0, keyRange - 1);

        int numberOfOps = 0;
        while (!RunController::startFlag.load()) {
            std::this_thread::yield();
        }

        while (!RunController::stopFlag.load()) {
            int key = keyDist(randKey);
            int chooseOperation = opDist(randOp);

            if (chooseOperation < addPercent) {
                set->add(key);
            } else if (chooseOperation < addPercent + removePercent) {
                set->remove(key);
            } else {
                set->contains(key);
            }

            numberOfOps++;
            RunController::globalOps.fetch_add(1, std::memory_order_relaxed);
        }

        results[threadId] = numberOfOps;
    }

    static void sanityRun(SetADT* set, int threadId, int keyRange, 
                          std::vector<std::vector<int>>& sanityAdds,
                          std::vector<std::vector<int>>& sanityRemoves) {
        std::mt19937 randOp(threadId + std::random_device{}());
        std::mt19937 randKey(threadId + 100 + std::random_device{}());
        std::uniform_int_distribution<int> opDist(0, 1);
        std::uniform_int_distribution<int> keyDist(0, keyRange - 1);

        std::vector<int> numberOfAdd(keyRange, 0);
        std::vector<int> numberOfRemove(keyRange, 0);

        while (!RunController::startFlag.load()) {
            std::this_thread::yield();
        }

        while (!RunController::stopFlag.load()) {
            int chooseOperation = opDist(randOp);
            int key = keyDist(randKey);

            if (chooseOperation == 1) {
                if (set->add(key)) {
                    numberOfAdd[key]++;
                    RunController::globalOps.fetch_add(1, std::memory_order_relaxed);
                } else if (set->remove(key)) {
                    numberOfRemove[key]++;
                    RunController::globalOps.fetch_add(1, std::memory_order_relaxed);
                }
            } else {
                if (set->remove(key)) {
                    numberOfRemove[key]++;
                    RunController::globalOps.fetch_add(1, std::memory_order_relaxed);
                } else if (set->add(key)) {
                    numberOfAdd[key]++;
                    RunController::globalOps.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        for (int i = 0; i < keyRange; i++) {
            sanityAdds[threadId][i] += numberOfAdd[i];
            sanityRemoves[threadId][i] += numberOfRemove[i];
        }
    }
};
