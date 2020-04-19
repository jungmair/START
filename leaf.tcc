#pragma once
/*
 * Simple utility functions for leafs
 */

static Node *makeLeaf(uintptr_t tid) {
    // Create a pseudo-leaf, that is stored directly in the pointer
    return reinterpret_cast<Node *>((tid << 2u) | 1u);
}

__attribute__((always_inline)) static inline uintptr_t getLeafValue(Node *node) {
    // The the value stored in the pseudo-leaf
    return reinterpret_cast<uintptr_t>(node) >> 2u;
}

__attribute__((always_inline)) static bool inline isLeaf(Node *node) {
    // Is the node a leaf?
    return (reinterpret_cast<uintptr_t>(node) & 1u) > 0;
}

static inline Node *markLeafDuplicate(Node *node) {
    //mark this leaf as duplicate
    return (Node * )(reinterpret_cast<uintptr_t>(node) | 2u);
}

__attribute__((always_inline)) static bool inline hasDuplicate(Node *node) {
    // Is the leaf marked as duplicate?
    return (reinterpret_cast<uintptr_t>(node) & 2u) > 0;
}
