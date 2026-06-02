#pragma once

#include <string>
#include <iostream>
#include <unistd.h>
#include <getopt.h>

struct TestConfig {
    int duration = 2;
    int numThreads = 5;
    int seed = 0;
    int searchFraction = 20;
    int insertFraction = 40;
    int deleteFraction = 40;
    int keyspaceSize = 100;
    std::string algo = "LockFreeList";
    bool testSanity = true;
    int warmUpTime = 2;

    void printHelp() {
        std::cout << "Usage: Test [options...]\n"
                  << "  -h, --help\n"
                  << "  -a, --algo  <Algorithm> (default=" << algo << ")\n"
                  << "  -t, --test-sanity <0|1> (default=" << testSanity << ")\n"
                  << "  -d, --duration <int> (default=" << duration << "s)\n"
                  << "  -n, --num-threads <int> (default=" << numThreads << ")\n"
                  << "  -s, --seed <int> (default=" << seed << ")\n"
                  << "  -r, --search-fraction <int> (default=" << searchFraction << "%)\n"
                  << "  -i, --insert-update-fraction <int> (default=" << insertFraction << "%)\n"
                  << "  -x, --delete-fraction <int> (default=" << deleteFraction << "%)\n"
                  << "  -w, --warm <int> (default=" << warmUpTime << "s)\n"
                  << "  -k, --keyspace-size <int> (default=" << keyspaceSize << ")\n";
    }

    bool parse(int argc, char** argv) {
        struct option longopts[] = {
            {"help", no_argument, nullptr, 'h'},
            {"duration", required_argument, nullptr, 'd'},
            {"num-threads", required_argument, nullptr, 'n'},
            {"seed", required_argument, nullptr, 's'},
            {"search-fraction", required_argument, nullptr, 'r'},
            {"insert-update-fraction", required_argument, nullptr, 'i'},
            {"delete-fraction", required_argument, nullptr, 'x'},
            {"keyspace1-size", required_argument, nullptr, 'k'},
            {"algo", required_argument, nullptr, 'a'},
            {"sanity", required_argument, nullptr, 't'},
            {"warm", required_argument, nullptr, 'w'},
            {nullptr, 0, nullptr, 0}
        };

        int c;
        while ((c = getopt_long(argc, argv, "hd:n:s:r:i:x:k:a:t:w:", longopts, nullptr)) != -1) {
            switch (c) {
                case 'h': printHelp(); return false;
                case 'a': algo = optarg; break;
                case 'd': duration = std::stoi(optarg); break;
                case 'n': numThreads = std::stoi(optarg); break;
                case 's': seed = std::stoi(optarg); break;
                case 'r': searchFraction = std::stoi(optarg); break;
                case 'i': insertFraction = std::stoi(optarg); break;
                case 'x': deleteFraction = std::stoi(optarg); break;
                case 'k': keyspaceSize = std::stoi(optarg); break;
                case 't': testSanity = std::stoi(optarg); break;
                case 'w': warmUpTime = std::stoi(optarg); break;
                case '?': printHelp(); return false;
            }
        }
        
        if (insertFraction + deleteFraction + searchFraction > 100) {
            std::cerr << "Fractions sum to > 100\n";
            return false;
        }
        return true;
    }
};
