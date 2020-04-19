#pragma once

#include "../../util/compile-time-switch.tcc"

//data for optimization with dynamic programming
struct dp_data {
    //during the dynamic programming, we need to store accumulated data for the last N tree levels
    //for each tree level, one levelInfo struct is maintainted. 
    struct levelInfo {
        size_t cost = 0;
        uint32_t childCount = 0;
        //how many leaves at this level
        uint32_t leaves = 0;
        //how many leaves in the total Subtree
        uint32_t totalLeaves = 0;
        //how many bytes of this level reside in L1/L2/L3 cache?
        uint32_t cached_l1 = 0;
        uint32_t cached_l2 = 0;
        uint32_t cached_l3 = 0;
        //8bit vector to store at wich levels multilevel nodes are located
        uint8_t mL = 0;

        levelInfo &operator+=(levelInfo &other) {
            //operator for accumulating levelInfos 
            mL |= other.mL;
            childCount += other.childCount;
            leaves += other.leaves;
            totalLeaves += other.totalLeaves;
            cost += other.cost;
            cached_l1 += other.cached_l1;
            cached_l2 += other.cached_l2;
            cached_l3 += other.cached_l3;

            return *this;
        }
    };
    //stores level informations for at most 9 levels
    levelInfo infos[9] = {};

    //general data:
    size_t next_free_level = 0;
    bool doReplace = false;

    //acumulate dp_datas of lower subtrees, toShift=how much bytes lower
    void addShifted(dp_data &other, size_t toShift) {
        for (size_t i = 0; i < 9ull - toShift; i++) {
            infos[i + toShift] += other.infos[i];
        }
    }
    //when we want to replace, we do not "slice" existing multilevel nodes into two
    //-> check if no multilevel node exists
    bool canReplace(int depth) {
        for (int i = 0; i < depth; i++) {
            for (int j = 0; j < 8; j++) {
                if (infos[i].mL & (1 << j) && i + j >= depth) {
                    return false;
                }
            }
        }
        return true;
    }

    levelInfo &top() {
        return infos[0];
    }
    //returns the total number of children, this subtree has until a certain depth
    size_t totalChildren(size_t depth) {
        depth = depth - 1;
        size_t totalChildren = infos[depth].childCount;
        for (size_t i = 0; i < depth; i++) {
            totalChildren += infos[i].leaves;
        }
        return totalChildren;
    }

};
