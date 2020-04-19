#include <gtest/gtest.h>

#include "datasets/cached.tcc"
#include "datasets/int.tcc"
#include "datasets/string.tcc"

#include "../art_impl.tcc"
#include "../config/config.tcc"
#include "../tuning/tuning.tcc"
/**
 * Tests range query functionality
 */
constexpr size_t default_tree_entries = 1000000;
static const char url[] = "datasets/eu-2005.urls";
//test organization
template<typename T>
struct ARTRangeTest : public ::testing::Test {
    using CONFIG=T;
};
template<class A, class B, size_t S>
struct TestConfig {
    using ARTConfig=A;
    using data_generator_t=B;
    static constexpr size_t testSize = S;
};
//for which node types should the test run
static constexpr auto NodeTypeList = hana::make_tuple(hana::type_c<Node4>, hana::type_c<Node16>, hana::type_c<Node48>,
                                                      hana::type_c<Node256>, hana::type_c<Rewired64K>,
                                                      hana::type_c<Rewired16M>, hana::type_c<MultiNode4>);
using NTL=decltype(NodeTypeList);
//abstract specification of test cases: art configuration + dataset
using denseTestType=TestConfig<ARTConfiguration<uint64_t, NTL, vec_storage_provider, no_statistics>, cached<dense_data_set<uint64_t>>, default_tree_entries>;
using randomTestType=TestConfig<ARTConfiguration<uint64_t, NTL, vec_storage_provider, no_statistics>, cached<random_int_data_set<uint64_t>>, default_tree_entries>;
using denseTestType32=TestConfig<ARTConfiguration<uint32_t, NTL, vec_storage_provider, no_statistics>, cached<dense_data_set<uint32_t>>, default_tree_entries>;
using randomTestType32=TestConfig<ARTConfiguration<uint32_t, NTL, vec_storage_provider, no_statistics>, cached<random_int_data_set<uint32_t>>, default_tree_entries>;
using zipfTestType=TestConfig<ARTConfiguration<uint64_t, NTL, vec_storage_provider, no_statistics>, cached<zipf_int_data_set>, default_tree_entries>;
using randomStringTestType=TestConfig<ARTConfiguration<std::string, NTL, vec_storage_provider, no_statistics>, random_string_data_set, default_tree_entries>;
using urlStringTestType=TestConfig<ARTConfiguration<std::string, NTL, vec_storage_provider, no_statistics>, file_string_data_set<url>, 800000>;
//list of all test cases
using ARTTestTypes = ::testing::Types<denseTestType, randomTestType, denseTestType32, randomTestType32, zipfTestType, randomStringTestType, urlStringTestType>;
TYPED_TEST_SUITE(ARTRangeTest, ARTTestTypes);

//execute for each test case
TYPED_TEST(ARTRangeTest, testIt) { // NOLINT(cert-err58-cpp)
    using ARTConfig=typename TestFixture::CONFIG::ARTConfig;
    using generator_t=typename TestFixture::CONFIG::data_generator_t;
    using RT=typename ARTConfig::RT_t;
    size_t tree_entries = TestFixture::CONFIG::testSize;
    //create dataset
    std::vector<typename ARTConfig::key_t> storage;
    storage.resize(tree_entries);
    generator_t::fill(storage);
    //create art + insert all entries
    RT config{storage};
    art_impl<ARTConfig> art(config);
    for (size_t i = 0; i < tree_entries; i++) {
        art.insertKey(i);
    }
    //ART->START
    tuning rs(art);
    rs.tune();
    //copy dataset and sort it 
    std::vector<typename ARTConfig::key_t> sortedStorage;
    sortedStorage.resize(tree_entries);
    std::copy(storage.begin(), storage.end(), sortedStorage.begin());
    std::sort(sortedStorage.begin(), sortedStorage.end());
    //seeded random generators
    std::uniform_int_distribution<size_t> distribution(0, tree_entries - 1);
    std::mt19937_64 generator(0);
    //10 ranges should be enough for now
    for (int i = 0; i < 10; i++) {
        //generate range to test
        size_t start = distribution(generator);
        size_t end = distribution(generator);
        if (start > end) {
            std::swap(start, end);
        }
        //compare range query behaviour with iteration through sorted vector
        art.range(sortedStorage[start], sortedStorage[end], [&](size_t tupleid) {
            if (start == end) {
                EXPECT_TRUE(false);
            } else {
                if (storage[tupleid] != sortedStorage[start]) {
                    EXPECT_TRUE(false);
                }
                EXPECT_EQ(storage[tupleid], sortedStorage[start]);
                start++;
            }

        });
    }
}
