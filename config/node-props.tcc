#pragma once

#include "../util/compile-time-switch.tcc"
#include "../tuning/util/costmodel.tcc"
#include "../tuning/util/dp_data.tcc"

template<class NT>
struct NodeProperties {
};

//properties object containing information about the range of allowed children and then 
class simple_level_size_props {
    size_t minLevels;
    size_t maxLevels;
    size_t minChildren_;
    size_t maxChildren_;
public:
    constexpr simple_level_size_props(size_t minLevels, size_t maxLevels, size_t minChildren_, size_t maxChildren_)
            : minLevels(minLevels), maxLevels(maxLevels), minChildren_(minChildren_), maxChildren_(maxChildren_) {}

public:
    constexpr bool supports_levels(size_t levels) {
        return levels >= minLevels && levels <= maxLevels;
    }

    constexpr bool supports_children(size_t /*levels*/, size_t children) {
        return children <= maxChildren_ && children >= minChildren_;
    }

    constexpr size_t minChildren(size_t /*levels*/) {
        return minChildren_;
    }

    constexpr size_t maxChildren(size_t /*levels*/) {
        return maxChildren_;
    }
};

//for nodes that directly store the child pointers without any modification
struct raw_child_ptr_behavior {
    static Node *raw(Node *val) {
        return val;
    }

    static Node *merge(Node */*oldVal*/, Node *newVal) {
        return newVal;
    }
};
//for nodes (e.g, rewired64k,rewired16m) that use pointer tagging
struct tagged_16_child_ptr_behavior {
    static Node *raw(Node *val) {
        //"unpack" pointer: remove tag
        uint64_t val_ = reinterpret_cast<uint64_t>(val);
        return reinterpret_cast<Node *>(val_ >> 16);
    }

    static Node *merge(Node *oldVal, Node *newVal) {
        //"pack" poinger: extract tag from old value and add it to new value
        uint64_t oldVal_ = reinterpret_cast<uint64_t>(oldVal);
        uint64_t newVal_ = reinterpret_cast<uint64_t>(newVal);
        return reinterpret_cast<Node *>((newVal_ << 16) | (oldVal_ & 0xffff));
    }
};

//usually nodes do not allow self growing
template<class T, class grow_type_>
struct simple_grow_props {
    using grow_type=grow_type_;

    static T *selfgrow(T *) {
        throw std::runtime_error("selfgrow not available!");
    }
};
//some node types may grow by itself (currently not used)
template<class T, class grow_type_>
struct self_grow_props {
    using grow_type=grow_type_;

    static T *selfgrow(T *in) {
        return T::grow(in);
    }
};
//usually nodes are allocated using new/delete
template<class T>
struct default_creator {
    static T *create(uint8_t levels) {
        return new T(levels);
    }

    static void destroy(T *n) {
        delete n;
    }
};
//but some nodes may need a special setup
template<class T>
struct cust_creator {
    static T *create(uint8_t levels) {
        return T::create(levels);
    }

    static void destroy(T *n) {
        return T::destroy(n);
    }
};
//some nodes can not be created generically at all. They are introduced during reorganization
template<class T>
struct only_destroy {
    static T *create(uint8_t /*levels*/) {
        throw;
    }

    static void destroy(T *n) {
        delete n;//still: all nodes must be destroyed
    }
};

//for most node node types, sizeof returns the node size
template<class T>
struct simple_size_estimater {
    static constexpr size_t getSize(size_t /*children*/) {
        return sizeof(T);
    }
};
//for rewired nodes, we may need to guess a size
struct rewired_size_estimater {
    static constexpr size_t getSize(size_t children) {
        return children * 8 + 64;
    }
};

//decide wether the node will be filled enough
template<typename T>
struct filled_enough {
    static bool apply(dp_data &/*replaceInfo*/) {
        return true;//default:yes
    }
};

template<>
struct filled_enough<Rewired64K> {
    static bool apply(dp_data &replaceInfo) {
        return replaceInfo.infos[1].childCount > 300;//simple heuristic
    }
};

template<>
struct filled_enough<Rewired16M> {
    static bool apply(dp_data &replaceInfo) {
        //simple heuristic
        return replaceInfo.infos[2].childCount / replaceInfo.top().childCount >= 300;
    }
};

template<class T, class grow_type>
struct SimpleProps {
    //default setting: 
    using child_ptr_behavior=raw_child_ptr_behavior;
    using grow_props=simple_grow_props<T, grow_type>;
    using creator=default_creator<T>;
    using size_estimater=simple_size_estimater<T>;
    simple_level_size_props levelSizeProps;
    simple_cost_model cost_model;
    bool is_rewired_node;

    constexpr SimpleProps(size_t minLevels, size_t maxLevels, size_t minChildren_, size_t maxChildren_,bool is_rewired_node=false)
            : levelSizeProps(minLevels, maxLevels, minChildren_, maxChildren_),
              cost_model(node_costs[T::NodeType]), is_rewired_node(is_rewired_node) {}
};

//define properties for all node types
template<>
struct NodeProperties<Node4> : public SimpleProps<Node4, Node16> {
    constexpr NodeProperties() : SimpleProps(1, 1, 1, 4) {}
};

template<>
struct NodeProperties<Node16> : public SimpleProps<Node16, Node48> {
    constexpr NodeProperties() : SimpleProps(1, 1, 5, 16) {}
};

template<>
struct NodeProperties<Node48> : public SimpleProps<Node48, Node256> {
    constexpr NodeProperties() : SimpleProps(1, 1, 17, 48) {}
};

template<>
struct NodeProperties<Node256> : public SimpleProps<Node256, Node256> {
    constexpr NodeProperties() : SimpleProps(1, 1, 49, 256) {}
};

template<>
struct NodeProperties<Rewired64K> : public SimpleProps<Rewired64K, NoNode> {
    constexpr NodeProperties() : SimpleProps(2, 2, 500, 1ull << 16, true) {}

    using child_ptr_behavior=tagged_16_child_ptr_behavior;
    using creator=only_destroy<Rewired64K>;
    using size_estimater=rewired_size_estimater;
};

template<>
struct NodeProperties<Rewired16M> : public SimpleProps<Rewired16M, NoNode> {
    constexpr NodeProperties() : SimpleProps(3, 3, 500, 1ull << 24, true) {}

    using child_ptr_behavior=tagged_16_child_ptr_behavior;
    using creator=only_destroy<Rewired16M>;
    using size_estimater=rewired_size_estimater;
};

template<>
struct NodeProperties<MultiNode4> : public SimpleProps<MultiNode4, NoNode> {
    constexpr NodeProperties() : SimpleProps(2, 4, 1, 4) {}
};