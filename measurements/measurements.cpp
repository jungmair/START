#include <iostream>
#include <chrono>
#include <emmintrin.h>
#include <random>
#include <unordered_set>
#include <x86intrin.h>
#include <fstream>

#include "../art_impl.tcc"
#include "../config/config.tcc"
#include "../tuning/tuning.tcc"

//also measure times for flat64K/flat16M nodes
#include "../nodes/simple/flat64k.tcc"
#include "../nodes/simple/flat16m.tcc"

template<>
struct NodeProperties<Flat64K> : public SimpleProps<Flat64K, NoNode> {
    constexpr NodeProperties() : SimpleProps(2, 2, 500, 1ull << 16) {}
};

template<>
struct NodeProperties<Flat16M> : public SimpleProps<Flat16M, NoNode> {
    constexpr NodeProperties() : SimpleProps(3, 3, 500, 1ull << 24) {}
};
//cache states to be measured
enum cache_status {
    not_cached = 0, header_cached = 1, full_cached = 2
};

double tscperns = 1;


#include "util/creator.tcc"
#include "util/clear_cache.tcc"

template<class T, class NTL>
struct measure_node {
    using Config=ARTConfiguration<uint64_t, NTL, vec_storage_provider, no_statistics>;
    using RT=typename Config::RT_t;
    T *underTest;
    Node4* before;
    std::vector<uint64_t> vec;
    RT c;
    art_impl<Config> art;
    std::vector<uint64_t> random_ints;
    std::vector<Node4 *> llpool;
    std::mt19937_64 generator;

    measure_node(size_t levels) : vec(), c(vec), art(c), generator(0) {
        //root node: node4
        before = new Node4(1);
        //"leaf" nodes: pool of 1000 Node4
        for (int i = 0; i < 1000; i++) {
            auto *after = new Node4(1);
            uint8_t x = 0;
            after->insert(&x, makeLeaf(1));
            llpool.push_back(after);
        }
        //how many keys should be generated?
        size_t keys = std::min((NodeProperties<T>().levelSizeProps.maxChildren(levels) * 3) / 4, 300ul);
        //key/child pointer pairs to be inserted into the node under test
        std::vector<std::pair<uint64_t, Node *>> values;
        std::uniform_int_distribution<size_t> distribution(0, 1000 - 1);
        //generate keys randomly
        generateKeys(keys, levels);
        for (size_t i = 0; i < keys; i++) {
            //assign "leaf" node randomly to one of the generated keys
            values.push_back({random_ints[i], llpool[distribution(generator)]});

        }
        underTest = creator<T>::create(values, art, levels);
        //do setup: connect before node with node under test + plug into art implementation
        uint8_t zero8 = 0;
        before->insert(&zero8, underTest);
        before->prefixLength = 8 - 2 - levels;
        memset(before->prefix, 0, 8);
        art.tree = before;
    }

    void generateKeys(size_t num, size_t levels) {
        //generate keys according to the following pattern
        // 1. the lowest byte is always zero
        // 2. the topmost (8-levels-1) bytes are zero

        //maximal number of distinct keys
        size_t maxKeys = 1ul << (levels * 8);
        
        if (num < maxKeys / 20) {
            //number of requested keys is much lower than the keyspace
            //strategy: draw keys from the keyspace and check wether it occured before
            std::unordered_set<size_t> set;
            random_ints.resize(num);
            std::uniform_int_distribution<size_t> distribution(0, num * 20);

            for (size_t i = 0; i < random_ints.size(); i++) {
                size_t k = distribution(generator) << 8ul;
                while (set.count(k)) {
                    k = distribution(generator) << 8ul;
                }
                random_ints[i] = k;
                set.insert(k);
            }
        } else {
            //number of requested keys is in the same order of magnitude than the keyspace
            //-> generate all possible keys
            for (size_t i = 0; i < maxKeys; i++) {
                random_ints.push_back(i << 8u);
            }
            //shuffle and take the first num keys
            std::shuffle(random_ints.begin(), random_ints.end(), generator);
            random_ints.resize(num);

        }
    }

    size_t nextKey() {
        std::uniform_int_distribution<size_t> distribution(0, random_ints.size() - 1);
        return random_ints[distribution(generator)];

    }

    ~measure_node() {
        //cleanup manually
        for(auto node4:llpool){
            delete node4;
        }
        delete underTest;
        delete before;
        //set to nullptr, to avoid double free
        art.tree = nullptr;
    }

};

//measure for NoNode under Test i.e. measure baseline
template<class NTL>
struct measure_node<NoNode, NTL> {
    using Config=ARTConfiguration<uint64_t, NTL, vec_storage_provider, no_statistics>;
    using RT=typename Config::RT_t;
    NoNode *underTest;
    std::vector<uint64_t> vec;
    RT c;
    art_impl<Config> art;
    std::vector<uint64_t> random_ints;

    explicit measure_node(size_t /*levels*/) : vec(), c(vec), art(c) {
        //do baseline setup Node4->Node4
        underTest = new NoNode(0);
        auto *after = new Node4(1);
        auto *before = new Node4(1);
        uint8_t zero8 = 0;
        before->insert(&zero8, after);
        after->insert(&zero8, makeLeaf(1));
        before->prefixLength = 6;
        memset(before->prefix, 0, 8);
        art.tree = before;
    }

    size_t nextKey() {
        return 0;
    }

    ~measure_node() {
        delete underTest;
    }

};


template<class T, size_t levels>
double measure(cache_status cs, size_t lookups = 100000) {

    static constexpr auto NodeTypeList = hana::make_tuple(hana::type_c<Node4>, hana::type_c<Node16>,
                                                          hana::type_c<Node48>, hana::type_c<Node256>,
                                                          hana::type_c<MultiNode4>,
                                                          hana::type_c<Rewired64K>,
                                                          hana::type_c<Rewired16M>,
                                                          hana::type_c<Flat64K>,
                                                          hana::type_c<Flat16M>);
    using NTL=decltype(NodeTypeList);
    measure_node<T, NTL> x{levels};
    //vector for storing individual measurements
    std::vector<size_t> measured_cycles{};
    for (size_t i = 0; i < lookups; i++) {
        //variables for lookup
        uintptr_t val = 0;
        bool duplicate = false;
        //clear node according to cache configuration
        clear_cache<T>::clear(x.underTest, cs, x.random_ints);
        //select key to lookup
        size_t k = x.nextKey();
        //measure cycles with rdtsc, surrounded by mfence
        _mm_mfence();
        size_t start = __rdtsc();
        //perform lookup, surrounded by mfence
        _mm_mfence();
        bool t = x.art.lookupVal(val, k, duplicate);
        _mm_mfence();
        size_t end = __rdtsc();
        _mm_mfence();
        //handle unsuccessfull lookup, should not happen
        if (!t)throw std::runtime_error("lookup failed during measurement!!");
        //register measured measured_cycles
        measured_cycles.push_back(end - start);
    }
    //calculate median of measured values
    std::nth_element(measured_cycles.begin(), measured_cycles.begin() + measured_cycles.size() / 2, measured_cycles.end());
    double median = measured_cycles[measured_cycles.size() / 2];
    //calculate and return nanoseconds
    return median / tscperns;
}

void updatetsc() {
    //the frequencyof tsc changes per processor
    auto start = __rdtsc();
    sleep(1);
    _mm_mfence();
    auto end = __rdtsc();
    //calculate resulting tsc per nanoseconds
    tscperns = ((double) (end - start)) / 1000000000;
}

std::ostream &operator<<(std::ostream &os, measured_node_costs &node_cost) {
    os << "{" << node_cost.inCache << "," << node_cost.headerCached << "," << node_cost.notCached << "}";
    return os;
}


using cost_table=measured_node_costs[256];

template<class T, size_t levels>
void resultsFor(const std::string &name, size_t baseline, cost_table &costTable, size_t lookups = 1000000) {
    //measure costs for each of three cases: full cached, header cached, not cached
    //subtract baseline to only measure the impact of the node under test
    double inCache = measure<T, levels>(full_cached, lookups) - baseline;
    double headerCached = measure<T, levels>(header_cached, lookups) - baseline;
    double notCached = measure<T, levels>(not_cached, lookups) - baseline;
    //print result to stdout
    std::cout << std::setw(30) << name << std::setw(20) << inCache
              << std::setw(20)
              << headerCached << std::setw(20)
              << notCached
              << std::endl;
    //write results into costTable for later usage
    costTable[T::NodeType] = {inCache, headerCached, notCached};
}

int main(int argc, char **argv) {
    if (argc != 2) {
        throw std::runtime_error("expecting destination");
    }
    //measure tsc per second
    updatetsc();
    //measure baseline: Node4->->Node4
    double baseline = measure<NoNode, 1>(full_cached, 10000000);
    //print measured baseline
    std::cout << "baseline:" << baseline << std::endl;
    //print header to stdout
    std::cout << std::setw(30) << "NT" << std::setw(20) << "in cache" << std::setw(20) << "header in cache"
              << std::setw(20) << "not in cache" << std::endl;
    cost_table costTable = {};
    //Execute measurements for every node type: Node4->Node under Test -> Node4
    resultsFor<Node4, 1>("4", baseline, costTable);
    resultsFor<Node16, 1>("16", baseline, costTable);
    resultsFor<Node48, 1>("48", baseline, costTable);
    resultsFor<Node256, 1>("256", baseline, costTable);
    resultsFor<MultiNode4, 4>("Node4ML(4)", baseline, costTable);
    resultsFor<Rewired64K, 2>("Rewired(2)", baseline, costTable, 100000);
    resultsFor<Rewired16M, 3>("Rewired(3)", baseline, costTable, 100000);
    resultsFor<Flat64K, 2>("Flat(2)", baseline, costTable, 100000);
    resultsFor<Flat16M, 3>("Flat(3)", baseline, costTable, 100000);
    //write results to a header file such that it can be included for usage in the cost model
    std::ofstream out(argv[1]);
    out << "static constexpr measured_node_costs node_costs[256] ={" << std::endl;
    for (int i = 0; i < 256; i++) {
        out << costTable[i] << "," << std::endl;
    }
    out << "};" << std::endl;
    return 0;
}