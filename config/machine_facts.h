#pragma once
// cache sizes are required to estimate which nodes reside in which caches.
// could be easily retrieved via cpuid
static constexpr size_t L1_CACHE_SIZE = 32768;
static constexpr size_t L2_CACHE_SIZE = 1048576;
static constexpr size_t L3_CACHE_SIZE = 1447034;

// ram/cache latencies are used for extrapolating node costs for nodes in l2/l3 caches
// this improves the cost model.
// these times are much harder to measure, but can be often looked up
static constexpr double L1_LATENCY = 1.3;
static constexpr double L2_LATENCY = 4;
static constexpr double L3_LATENCY = 14;
static constexpr double RAM_LATENCY = 60;
