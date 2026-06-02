#pragma once

#include <vector>
#include <cmath>
#include <random>

class ZipfDistribution {
private:
    std::discrete_distribution<int> dist;
public:
    ZipfDistribution(int n, double alpha = 1.0) {
        std::vector<double> weights(n);
        double c = 0.0;
        for (int i = 1; i <= n; i++) {
            c += (1.0 / std::pow(i, alpha));
        }
        for (int i = 1; i <= n; i++) {
            weights[i - 1] = (1.0 / std::pow(i, alpha)) / c;
        }
        dist = std::discrete_distribution<int>(weights.begin(), weights.end());
    }

    template<class Generator>
    int operator()(Generator& g) {
        return dist(g);
    }
};
