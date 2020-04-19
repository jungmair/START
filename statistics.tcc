#pragma once

#include <iomanip>
#include<iostream>

/*
In some cases, we want to track statistics but this should not influence runtime in other situations.
-> two different structs that implement the same functions. One of the two implementations no_statistics/statistics
will be set at compile time
*/

//we want to track some special events, currently only full key loads during lookup
enum statistics_event {
    FULL_KEY_LOADS
};

//no_statistics: implements the same functions but does nothing
struct no_statistics {
    void registerEvent(statistics_event) {

    }

    template<class NT>
    void registerNodeCreation(NT *) {

    }

    template<class NT>
    void registerNodeDestruction(NT *) {

    }

    void registerRewired64K(size_t, size_t) {

    }

    void registerRewired16M(size_t, size_t) {

    }

    size_t getSize() {
        return 0;
    }

};
//do not print anything for no_statistics
inline std::ostream &operator<<(std::ostream &os, no_statistics &/*s*/) {
    return os;
}
//struct for storing statistics related to rewired nodes
struct rewired_node_data{
    std::string name;
    size_t children;
    size_t bytes;
    rewired_node_data(const std::string &name):name(name){}
};
inline std::ostream &operator<<(std::ostream &os, rewired_node_data &rnd) {
    //print rewired node data
    os<<rnd.name<<": { children:"<<rnd.children<<", bytes:"<<rnd.bytes<<", bytes per children:"<<(rnd.bytes/rnd.children)<<"}";
    return os;
}
struct statistics {
    //helper for node sizes
    std::map<size_t, size_t> sizes = {{0, sizeof(Node4)},
                                        {1, sizeof(Node16)},
                                        {2, sizeof(Node48)},
                                        {3, sizeof(Node256)},
                                        {6, sizeof(MultiNode4)}};
    //counter: how often did the FULL_KEY_LOADS event happen?
    size_t full_key_loads;
    //statistics for rewired nodes
    rewired_node_data rewired64K={"Rewired64K"};
    rewired_node_data rewired16M={"Rewired16M"};
    //how often do nodes with a certain number of levels occur
    std::map<std::pair<size_t, size_t>, size_t> nodeCounts;

    statistics() : full_key_loads(0), nodeCounts{} {}

    void registerEvent(statistics_event event) {
        //handles event (currently only FULL_KEY_LOADS)
        if (event == FULL_KEY_LOADS) {
            full_key_loads++;
        }
    }

    template<class NT>
    void registerNodeCreation(NT *n) {
        //update node counts
        nodeCounts[{n->type, n->levels}]++;
    }

    template<class NT>
    void registerNodeDestruction(NT *n) {
        //update node counts
        nodeCounts[{n->type, n->levels}]--;
    }

    void registerRewired64K(size_t children, size_t bytes) {
        //update data for rewired64K
        rewired64K.children += children;
        rewired64K.bytes += bytes;
    }

    void registerRewired16M(size_t children, size_t bytes) {
        //update data for rewired16M
        rewired16M.children += children;
        rewired16M.bytes += bytes;
    }

    size_t getSize() {
        //start with bytes of rewired nodes
        size_t sum = rewired64K.bytes + rewired16M.bytes;
        //then sum up sizes for all other nodes
        for (auto x:nodeCounts) {
            auto p = x.first;
            sum += sizes[p.first] * x.second;
        }
        return sum;
    }

};
//print statistics
inline std::ostream &operator<<(std::ostream &os, statistics &s) {

    std::cout << "Statistics:" << std::endl;
    std::cout << "Key loads (during lookups):" << s.full_key_loads << std::endl;
    //print data for rewired nodes
    std::cout << s.rewired64K<<std::endl;
    std::cout << s.rewired16M<<std::endl;
    //print header for overall nodes data
    std::cout << std::setw(40) << "node type" << std::setw(10) << "levels" << std::setw(50) << "count" << std::endl;
    //maps node ids to human readable names
    std::map<size_t, std::string> names = {{0, "Node4"},
                                          {1, "Node16"},
                                          {2, "Node48"},
                                          {3, "Node256"},
                                          {4, "Rewired64K"},
                                          {5, "Rewired16M"},
                                          {6, "Node4ML"}};
    //print counts for every node type
    for (auto x:s.nodeCounts) {
        auto p = x.first;
        std::cout << std::setw(40) << names[p.first] << std::setw(10) << p.second << std::setw(50) << x.second
                  << std::endl;
    }
    return os;
}
