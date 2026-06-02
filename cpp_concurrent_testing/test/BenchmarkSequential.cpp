#include "TestConfig.hpp"
#include "RunOperations.hpp"
#include "RunController.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>

#include "StructureFactory.hpp"

int main(int argc, char** argv) {
    TestConfig config;
    if (!config.parse(argc, argv)) return 1;

    auto set = createSet(config.algo);
    if (!set) {
        std::cerr << "Algorithm " << config.algo << " not implemented.\n";
        return 1;
    }

    std::vector<int> results(1, 0);
    RunController::reset();

    std::cout << "Running Sequential Benchmark for Set\n";
    RunController::startFlag.store(true);

    auto startObj = std::chrono::high_resolution_clock::now();
    
    // Run sequentially on main thread
    std::thread t1(RunOperations::benchMarkRun, set.get(), 0, config.insertFraction, 
                   config.deleteFraction, config.keyspaceSize, std::ref(results));
    
    std::this_thread::sleep_for(std::chrono::seconds(config.duration));
    RunController::stopFlag.store(true);
    t1.join();

    auto endObj = std::chrono::high_resolution_clock::now();
    double exactTime = std::chrono::duration<double>(endObj - startObj).count();
    
    std::cout << "Sequential Throughput = " << (results[0] / exactTime) << " Ops/sec\n";
    return 0;
}
