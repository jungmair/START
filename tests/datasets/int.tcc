#pragma once
//utility for generating integer datasets according to random, dense and zipfian distributions
#include <unordered_set>
#include <random>
#include "util/random_int.tcc"
#include "util/zipf.h"

template<class int_t>
struct random_int_data_set {
    using type=int_t;

    static void fill(std::vector<int_t> &vec) {
        //generate unique random numbers
        random_int<int_t> r;//use simple wrapper
        //use set for making the datset unique
        std::unordered_set<int_t> set;
        for (size_t i = 0; i < vec.size(); i++) {
            int_t val = r.next();
            while (set.count(val)) {
                val = r.next();
            }
            vec[i] = val;
            set.insert(val);
        }
    }
};

template<class int_t>
struct dense_data_set {
    using type=int_t;

    static void fill(std::vector<int_t> &vec) {
        //fill vector with numbers 0,... and shuffle them
        std::mt19937_64 generator;
        generator.seed(0);
        for (size_t i = 0; i < vec.size(); i++) {
            vec[i] = i;
        }
        std::shuffle(vec.begin(), vec.end(), generator);
    }
};

struct zipf_int_data_set {
    using type=uint64_t;

    static void fill(std::vector<uint64_t> &vec) {
        std::mt19937 generator;
        //use zipf_distribution class
        zipf_distribution<uint64_t> zipfDistribution{std::numeric_limits<uint64_t>::max(), 1.0};
        generator.seed(0);
        //use set for making the dataset unique
        std::unordered_set<uint64_t> set;
        for (size_t i = 0; i < vec.size(); i++) {
            uint64_t val = zipfDistribution(generator);
            while (set.count(val)) {
                val = zipfDistribution(generator);
            }
            vec[i] = val;
            set.insert(val);
        }
    }
};
