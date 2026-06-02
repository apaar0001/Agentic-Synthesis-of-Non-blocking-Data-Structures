#include "TestConfig.hpp"
#include "RunOperations.hpp"
#include "RunController.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <random>

#include "StructureFactory.hpp"

int main(int argc, char** argv) {
    TestConfig config;
    if (!config.parse(argc, argv)) return 1;

    std::cout << "Experiment: Algo:" << config.algo 
              << ", Dist:" << config.searchFraction << "/" << config.insertFraction << "/" << config.deleteFraction
              << ", Duration:" << config.duration << "s"
              << ", Threads:" << config.numThreads << ", Keys:" << config.keyspaceSize << "\n";

    auto set = createSet(config.algo);
    if (!set) {
        std::cerr << "Algorithm " << config.algo << " not implemented yet.\n";
        return 1;
    }

    std::cerr << "The experiment: Algo:" << config.algo 
              << ", Distribution: search " << config.searchFraction 
              << " insert " << config.insertFraction 
              << " delete " << config.deleteFraction
              << ", Duration(s):" << config.duration 
              << ", Threads:" << config.numThreads 
              << ", KeyRange(starting at 0):" << config.keyspaceSize << "\n";

    // Initialize presentKeys array to track initial state
    std::vector<int> presentKeys(config.keyspaceSize, 0);
    
    // Initialize half of the keys into the set
    std::mt19937 rd(0);
    std::uniform_int_distribution<int> keyDist(0, config.keyspaceSize - 1);
    for (int i = 0; i < config.keyspaceSize / 2;) {
        int key = keyDist(rd);
        bool added = set->add(key);
        if (added) {
            i++;
            presentKeys[key]++;
        }
    }

    // Initialize sanity tracking arrays
    std::vector<std::vector<int>> sanityAdds(config.numThreads, std::vector<int>(config.keyspaceSize, 0));
    std::vector<std::vector<int>> sanityRemoves(config.numThreads, std::vector<int>(config.keyspaceSize, 0));

    std::vector<std::thread> threads;
    RunController::reset();

    for (int i = 0; i < config.numThreads; i++) {
        threads.emplace_back(RunOperations::sanityRun, set.get(), i, config.keyspaceSize, 
                             std::ref(sanityAdds), std::ref(sanityRemoves));
    }

    RunController::startFlag.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::seconds(config.duration));
    RunController::stopFlag.store(true, std::memory_order_release);
    
    for (auto& t : threads) t.join();

    // Perform sanity check
    bool failedSanity = false;
    for (int k = 0; k < config.keyspaceSize; k++) {
        int keyAdded = presentKeys[k];
        int keyRemoved = 0;
        int initialKeyAdded = keyAdded;
        
        for (int tid = 0; tid < config.numThreads; tid++) {
            keyAdded += sanityAdds[tid][k];
            keyRemoved += sanityRemoves[tid][k];
        }

        if (set->contains(k)) {
            if (keyAdded != keyRemoved + 1) {
                std::cout << "\033[32mFirst Sanity Test failed at key " << k 
                          << ", keyAdded = " << keyAdded 
                          << ", keyRemoved = " << keyRemoved << ".\n";
                std::cout << "Initial Key Added " << initialKeyAdded << "\n\033[0m";
                failedSanity = true;
            }
        } else if (keyAdded != keyRemoved) {
            std::cout << "\033[32mSecond Sanity Test failed at key " << k 
                      << ", keyAdded = " << keyAdded 
                      << ", keyRemoved = " << keyRemoved << ".\n";
            std::cout << "Initial Key Added " << initialKeyAdded << "\n\033[0m";
            failedSanity = true;
        }
    }

    if (!failedSanity) {
        std::cout << "Sanity Test Passed\n";
        return 0;
    } else {
        std::cout << "Sanity Test Failed\n";
        return 1;
    }
}
