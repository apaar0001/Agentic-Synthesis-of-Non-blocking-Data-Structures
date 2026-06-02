#include "TestConfig.hpp"
#include "../utils/SetADT.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <chrono>
#include <atomic>

#include "StructureFactory.hpp"

int main(int argc, char** argv) {
    TestConfig config;
    if (!config.parse(argc, argv)) return 1;

    auto set = createSet(config.algo);
    if (!set) {
        std::cerr << "Algorithm " << config.algo << " not imp.\n";
        return 1;
    }

    constexpr int TOTAL_OPS = 1000000;
    std::atomic<long> taskCounter{0};
    
    auto worker = [&]() {
        while (true) {
            long task = taskCounter.fetch_add(1);
            if (task >= TOTAL_OPS) break;
            
            if (task % 3 == 0) set->contains(1);
            else if (task % 3 == 1) set->add(1);
            else set->remove(1);
        }
    };

    auto startConc = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    for (int i = 0; i < config.numThreads; i++) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) t.join();
    auto endConc = std::chrono::high_resolution_clock::now();
    
    double concTimeMs = std::chrono::duration<double, std::milli>(endConc - startConc).count();
    
    std::cout << "Concurrent time = " << concTimeMs << " ms\n";
    std::cout << "Completed ops   = " << std::min((long)TOTAL_OPS, taskCounter.load()) << "\n";
    
    if (concTimeMs <= config.duration * 1000) {
        std::cout << "RESULT: LIKELY LOCK-FREE\n";
    } else {
        std::cout << "RESULT: LIKELY LOCK-BASED (did not finish in time)\n";
    }
    return 0;
}
