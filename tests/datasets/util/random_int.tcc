#pragma once
//simple wrapper around uniform_int_distribution and mt19937_64. For easy generation of uniformly distributed random numbers
#include <random>

template<typename T>
class random_int {
    std::mt19937_64 generator;
    std::uniform_int_distribution<T> distribution;
public:
    random_int() : distribution(std::numeric_limits<T>::min(), std::numeric_limits<T>::max()) {
        generator.seed(0);
    }

    T next() {
        return distribution(generator);
    }

    void reset() {
        generator.seed(0);
    }
};
