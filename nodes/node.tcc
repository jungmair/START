#pragma once
#include<exception>


enum insert_result_t {
    //three different cases possible:
    // 1. insert succeeded
    // 2. insert is not possible
    // 3. the node implementation could grow "itself" to be able to insert
    success, failed, selfgrow
};

//base class for all nodes
struct Node {
public:
    //how many direct children?
    uint32_t count;
    //which node type?
    uint8_t type;
    //how many levels does th
    uint8_t levels;
    //is there a prefix/how long is it
    uint16_t prefixLength;
    union {
        //store the first 8 bytes of a prefix
        uint8_t prefix[8];
        //alternatively: store 64-bit integer in same slot
        uint64_t prefix_int;
    };

    explicit Node(uint8_t type, uint8_t levels) : count(0), type(type), levels(levels), prefixLength(0) {
    }

    void movePrefixFrom(Node *n) {
        //move prefix from one node to another
        this->prefixLength = n->prefixLength;
        this->prefix_int = n->prefix_int;
        n->prefix_int = 0;
        n->prefixLength = 0;
    }


/*
    Each node type must implement the following functions. They are not virtual functions, but are called through
    template magic

    Node **findChildPtr(uint8_t *key) {
        //returns a pointer to the internal location of the matching child pointer or nullptr if no  child matches
    }


    __attribute__((always_inline)) Node *lookup(uint8_t *keyByteArr) {
        //returns the child pointer matching the key, or nullptr otherwise
    }

    insert_result_t insert(uint8_t *key, Node *child) {
        //inserts a new pair of key part and child pointer
        //returns success/failed/selfgrow
    }

    template<typename F>
    void iterateOver(F f) {
        //iterates over all children and calls f with a pointer to the key parts and the child pointer
    }
*/
};

/**
 * Sometimes we want to express a "NoNode" type.
 * Still, this node type should never be placed into the tree
 */
struct NoNode : public Node {

    NoNode(size_t levels) : Node(255, levels) {

    }
    Node **findChildPtr(uint8_t */*key*/) {
        throw std::runtime_error("findChildPtr called on NoNode!");
    }


    __attribute__((always_inline)) Node *lookup(uint8_t */*keyByteArr*/) {
        throw std::runtime_error("lookup called on NoNode!");
    }

    insert_result_t insert(uint8_t */*key*/, Node */*child*/) {
        throw std::runtime_error("insert called on NoNode!");
    }

    template<typename F>
    void iterateOver(F /*f*/) {
        throw std::runtime_error("iterateOver called on NoNode!");
    }
};
