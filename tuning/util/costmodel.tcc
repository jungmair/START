#pragma once

#include "../../config/machine_facts.h"

//helper struct for including the measured node costs
struct measured_node_costs {
    double inCache;
    double headerCached;
    double notCached;
};
//include the freshly measured node costs
#include "../../config/node_costs.h"

//enum for storing the caching state of an object:
enum caching_state {
    L1 = 0,//stored in L1 cache
    L2 = 1,//stored in L2 cache
    L3 = 2,//stored in L3 cache
    NO = 3 //not cached at all -> only in main memory
};
//input for the costmodel
struct cost_parameter {
    //how many lookups?
    size_t lookups;
    //what is the estimated caching state of the node's header
    caching_state headerCacheStatus;
    //what is the estimated caching state of the node's body
    caching_state mainCacheStatus;
    //for how many lookups do we have to retrieve the full key?
    size_t keyLookups;
};

struct simple_cost_model {
    measured_node_costs node_costs;

    constexpr simple_cost_model(measured_node_costs node_costs) : node_costs(node_costs) {}


    double getAdditionalCost(caching_state cache_status, double memoryAccesses) {
        double realLatency = 0;
        //get latency for a lookup in L1/L2/L3
        switch (cache_status) {
            case L1:
                realLatency = L1_LATENCY;
                break;
            case L2:
                realLatency = L2_LATENCY;
                break;
            case L3:
                realLatency = L3_LATENCY;
                break;
            default:
                throw std::runtime_error("wrong cache status...");
        }
        //extrapolate
        return (realLatency - L1_LATENCY) * memoryAccesses;
    }

    constexpr double getNodeCost(const measured_node_costs &costs, caching_state header, caching_state mainPart) {
        //what is the influence of the header?
        double headerInfluence = costs.notCached - costs.headerCached;
        //whole node is uncached -> return costs measured for main memory
        if (mainPart == NO && header == NO) {
            return costs.notCached;
        }
        //header cached, body not cached => use headerCached measurement as base and add costs if in L2/L3
        if (mainPart == NO) {
            return costs.headerCached + getAdditionalCost(header, headerInfluence / RAM_LATENCY);
        }
        //both, header and body are cached somewhere -> use inCache measurement + compensate for L2/L3 measurements
        return costs.inCache + getAdditionalCost(header, headerInfluence / RAM_LATENCY) +
               getAdditionalCost(mainPart, costs.headerCached / RAM_LATENCY);

    }

    constexpr size_t getCost(cost_parameter &param) {
        return param.lookups * getNodeCost(node_costs, param.headerCacheStatus, param.mainCacheStatus) + // lookups* lookup_costs
               param.keyLookups * RAM_LATENCY;//add cost for looking up the full key
    }
};