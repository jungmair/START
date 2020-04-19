#pragma once

template<class C>
class art_impl;

#include <bitset>
#include <iostream>
#include "nodes/nodes.tcc"
#include "nodes/migrate/migrate.tcc"
#include "util/compile-time-switch.tcc"
#include "config/node-props.tcc"
#include "statistics.tcc"
#include "leaf.tcc"
namespace hana = boost::hana;
using namespace hana::literals;

template<typename Config>
class art_impl {
    using K=typename Config::key_t;
    using KP=typename Config::key_props_t;
    using NT=decltype(Config::nodeTypeList);
    using Statistics=typename Config::statistics_t;
    using start_node_t=Node4;
    using RT=typename Config::RT_t;

private:
    //root
    Node *tree;
    //runtime config
    RT &rt;

    inline bool
    leafMatches(Node *leaf, const uint8_t key[], unsigned keyLength, unsigned depth, unsigned maxKeyLength) {
        // Check if the key of the leaf is equal to the searched key
        if (depth != keyLength) {
            uint8_t leafKey[maxKeyLength];
            rt.storage.loadKey(getLeafValue(leaf), leafKey);
            for (unsigned i = depth; i < keyLength; i++)
                if (leafKey[i] != key[i])
                    return false;
        }
        return true;
    }

    __attribute__((always_inline)) Node *
    lookup(Node *n, uint8_t *key, unsigned keyLength, unsigned maxKeyLength, unsigned depth = 0) {
        // Lookup the key
        //if we reach a leaf, are we certain, that the key matches?
        bool insecurity = false;
        if (n == nullptr)
            return nullptr;
        while (true) {
            if (isLeaf(n)) {
                //check if leaf matches
                if ((depth == keyLength && !insecurity) || leafMatches(n, key, keyLength, depth, maxKeyLength)) {
                    if (!(depth == keyLength && !insecurity)) {
                        //extra key lookup had to be done -> register for statistics
                        rt.statistics.registerEvent(FULL_KEY_LOADS);
                    }
                    //leaf matches -> return value;
                    return n;
                }
                //leaf, but does not match 
                return nullptr;
            }
            //handle prefix if existent
            if (n->prefixLength != 0) {
                for (unsigned pos = 0; pos < std::min(n->prefixLength, (uint16_t) 8); pos++)
                    if (key[depth + pos] != n->prefix[pos])
                        return NULL;
                if (n->prefixLength > 8) {
                    insecurity = true;
                }
                depth += n->prefixLength;
            }
            //perform lookup depending on the node type
            compile_time_switch<NT>(n, [&n, key, &depth](auto x) {
                n = x->lookup(&key[depth]);
                depth += x->levels;
            });
            //exit, when nullptr is reached
            if (n == nullptr) {
                return nullptr;
            }
        }
    }

    void loadFullKey(Node *n, uint8_t *arr) {
        // for a given (inner) node, return one arbitrary key stored underneath n
        Node *leaf = findLeafFor(n);
        uintptr_t tupleId = getLeafValue(leaf);
        rt.storage.loadKey(tupleId, arr);
    }

    size_t mismatch_prefix(Node *node, uint8_t *key, unsigned depth, unsigned keyLength) {
        //checks if prefix matches for sure
        // -> if whole prefix does not fit into node -> load one arbitrary full key for comparison
        size_t pos = 0;
        size_t max_bytes = std::min(keyLength - depth, (unsigned) node->prefixLength);
        uint8_t fullKey[KP::maxKeySize()];
        loadFullKey(node, fullKey);
        //compare prefixes
        uint8_t *prefixPtr = node->prefixLength <= 8 ? node->prefix : &fullKey[depth];
        for (; pos < max_bytes; pos++) {
            if (key[depth + pos] != prefixPtr[pos])
                return pos;
        }
        return pos;
    }

    bool insert(Node *&node, uint8_t key[], uintptr_t value, unsigned maxKeyLength, unsigned depth = 0) {
        // Insert the leaf value into the tree
        if (node == nullptr) {
            node = makeLeaf(value);
            return true;
        }
        if (isLeaf(node)) {
            //leaf reached
            if (leafMatches(node, key, maxKeyLength, depth, maxKeyLength)) {
                //leaf matches -> duplicate
                node = markLeafDuplicate(node);
                return true;
            }
            //leaf did not match -> split at some byte
            //load existing key
            uint8_t existingKey[maxKeyLength];
            rt.storage.loadKey(getLeafValue(node), existingKey);
            //find "splitting" byte
            unsigned newPrefixLength = 0;
            while (existingKey[depth + newPrefixLength] == key[depth + newPrefixLength])
                newPrefixLength++;
            //create new Node
            auto *newNode = NodeProperties<start_node_t>::creator::create(1);
            rt.statistics.registerNodeCreation(newNode);
            //set prefix accordingly
            newNode->prefixLength = newPrefixLength;
            std::memcpy(newNode->prefix, key + depth, std::min(newPrefixLength, 8u));
            //insert both old and new leaf into the newly created node
            newNode->insert(&existingKey[depth + newPrefixLength], node);
            newNode->insert(&key[depth + newPrefixLength], makeLeaf(value));
            node = newNode;
            return true;
        }
        if (node->prefixLength) {
            //node has prefix
            unsigned mismatchPos = mismatch_prefix(node, key, depth, maxKeyLength);
            if (mismatchPos != node->prefixLength) {
                //prefix does not match at some point -> split
                //load prefixes 
                uint8_t tmpPrefix[8];
                std::memcpy(tmpPrefix, node->prefix, 8);
                bool prefixLenTooBig = node->prefixLength > 8;
                uint8_t fullKey[KP::maxKeySize()];
                if (prefixLenTooBig) {
                    loadFullKey(node, fullKey);
                }
                uint8_t *prefixPtr = prefixLenTooBig ? &fullKey[depth] : tmpPrefix;
                // Prefix differs, create new node that "splits" the prefix
                auto *newNode = NodeProperties<start_node_t>::creator::create(1);
                rt.statistics.registerNodeCreation(newNode);
                //share prefix as long as possible
                newNode->prefixLength = mismatchPos;
                std::memcpy(newNode->prefix, prefixPtr, std::min(mismatchPos, 8u));
                // Break up remaining part of prefix
                node->prefixLength -= (mismatchPos + 1);
                memmove(node->prefix, prefixPtr + mismatchPos + 1, std::min(node->prefixLength, (uint16_t) 8));
                newNode->insert(prefixPtr + mismatchPos, node);
                newNode->insert(&key[depth + mismatchPos], makeLeaf(value));//insert new leaf directly
                node = newNode;
                return true;
            }
            //prefix does match -> skip prefix
            depth += node->prefixLength;
        }
        Node **child = nullptr;
        size_t nodeLevels = node->levels;
        //find child pointer depending on node type
        compile_time_switch<NT>(node, [&node, key, depth, &child, &nodeLevels](auto x) {
            child = x->findChildPtr(&key[depth]);
        });
        if (child != nullptr && *child) {
            //child pointer is there -> recurse
            Node *newVal;
            //"unpack" pointer value depending on node type
            compile_time_switch<NT>(node, [&](auto x) {
                using localNodeType=typename std::remove_pointer<decltype(x)>::type;
                newVal = NodeProperties<localNodeType>::child_ptr_behavior::raw(*child);
            });
            //recursively insert
            bool res = insert(newVal, key, value, maxKeyLength, depth + nodeLevels);
            //"pack" pointer value back again
            compile_time_switch<NT>(node, [&](auto x) {
                using localNodeType=typename std::remove_pointer<decltype(x)>::type;
                *child = NodeProperties<localNodeType>::child_ptr_behavior::merge(*child, newVal);
            });
            //done
            return res;
        }
        //no child pointer to follow -> insert leaf directly
        Node *newNode = makeLeaf(value);
        bool res = true;
        //perform insert depending on node type
        compile_time_switch<NT>(node, [&](auto x) {
            //type of the node
            using localNodeType=typename std::remove_pointer<decltype(x)>::type;
            //node type for migration, if node overflows
            using growType=typename NodeProperties<localNodeType>::grow_props::grow_type;

            //try insert
            insert_result_t insertResult = x->insert(&key[depth], newNode);
            if (insertResult == success) {
                return;//worked -> done
            } else if (insertResult == selfgrow) {
                //try selfgrow
                localNodeType *n2 = NodeProperties<localNodeType>::grow_props::selfgrow(x);
                if (n2 == x) {
                    //selfgrow did not work...
                    insertResult = failed;
                } else {
                    //selfgrow did work -> try insert again
                    node = n2;
                    insertResult = n2->insert(&key[depth], newNode);
                }
            }

            if (insertResult == failed) {
                //insert went wrong -> treat as overflow
                size_t levelsBefore = node->levels;
                Node *n2 = migrate<localNodeType, growType, Config>::apply((localNodeType *) node, *this,
                                                                                    depth, node->levels);
                node = n2;

                if (n2->levels != levelsBefore) {
                    //if we reduced multilevel node -> redo insertion, alternative would be too complicated 
                    res = false;
                } else {
                    //simple insert depending on the node type
                    compile_time_switch<NT>(n2, [&n2, key, depth, newNode, this](auto y) {
                        y->insert(&key[depth], newNode);
                    });
                }
                return;
            }
        });
        return res;
    }

    void cleanup(Node *n) {
        //recursively cleanup all nodes in the the tree
        if (!n)return;
        if (isLeaf(n))return;
        compile_time_switch<NT>(n,
                                [this](auto x) {
                                    x->iterateOver([this](uint8_t *, Node *&child) {
                                        cleanup(child);
                                    });
                                    rt.statistics.registerNodeDestruction(x);
                                    delete x;
                                });
    }
    //recursive funcion for finding a end marker
    Node *findEnd_(Node *n, uint8_t *keyBytes, size_t depth) {
        //skip prefixes
        if (n->prefixLength != 0) {
            depth += n->prefixLength;
        }
        Node *result = nullptr;
        compile_time_switch<NT>(n, [&](auto x) {
            using localNodeType=typename std::remove_pointer<decltype(x)>::type;
            //find entry for key Bytes
            Node **child = x->findChildPtr(&keyBytes[depth]);
            if (child != nullptr && *child) {
                //exact match found for entry
                //unpack Node* if it is packed
                Node *nchild = NodeProperties<localNodeType>::child_ptr_behavior::raw(*child);
                if (isLeaf(nchild)) {
                    //leaf-> use this value as end marker
                    result = nchild;
                } else {
                    //procede recursively
                    result = findEnd_(nchild, keyBytes, depth + x->levels);
                }
            } else {
                //no exact match found -> use first_geq function to determine the next match
                using iterator_t=typename localNodeType::iterator_t;
                iterator_t it = x->first_geq(&keyBytes[depth]);
                if (it == x->end()) {
                    result = nullptr;
                } else {
                    result = *it;
                }
            }
        });
        return result;
    }
    //find the first Node* that is >=val -> end marker
    Node *findEnd(const K &val) {
        uint8_t arr[KP::maxKeySize()+8];
        KP::keyToBytes(val, arr);
        return findEnd_(tree, arr, 0);
    }

    Node *findLeafFor(Node *n) {
        //find a representative leaf underneath n (simple recursion)
        if (isLeaf(n))return n;
        Node *res;
        compile_time_switch<NT>(n, [&](auto x) {
            res = findLeafFor(*x->begin());
        });
        return res;

    }

    //recursive implementation of range queries
    template<class F>
    void range_(Node *n, Node *end, uint8_t *keyBytes, size_t depth, bool &foundStart, bool &foundEnd, const F &f) {
        //skip prefixes
        if (n->prefixLength != 0) {
            depth += n->prefixLength;
        }
        //range queris depend on node type
        compile_time_switch<NT>(n, [&](auto x) {
            //current node type
            using localNodeType=typename std::remove_pointer<decltype(x)>::type;
            using iterator_t=typename localNodeType::iterator_t;
            iterator_t it;
            if (!foundStart) {
                //we may have to skip some entries not in the specified range
                //-> use first_geq for skipping locally
                it = x->first_geq(&keyBytes[depth]);
                Node **child = x->findChildPtr(&keyBytes[depth]);
                if (child != nullptr && *child) {
                    //procede recursively to skip all necessary
                    Node *nchild = NodeProperties<localNodeType>::child_ptr_behavior::raw(*child);
                    if (isLeaf(nchild)) {
                        //done: leaf is start 
                        foundStart = true;
                    } else {
                        //procede recursively for child pointer
                        range_(nchild, end, keyBytes, depth + x->levels, foundStart, foundEnd, f);
                        if (foundEnd)return;//itt may happen that start=end
                        ++it;
                    }
                } else {
                    //can not skip more
                    foundStart = true;
                }

            } else {
                it = x->begin();
            }
            //iterate over entries until end is reached
            while (it != x->end() && *it != end) {
                Node *child = *it;
                if (isLeaf(child)) {
                    f(getLeafValue(child));//execute lambda for leaf
                } else {
                    //child pointer -> visit recursively
                    range_(child, end, keyBytes, depth + x->levels, foundStart, foundEnd, f);
                    if (foundEnd)return;//break if end was found
                }
                ++it;
            }
            //end was reached locally -> set global flag
            if (it !=x->end()&&*it == end)foundEnd = true;
        });
    }

public:
    explicit art_impl(RT &config): rt(config) {
        tree = nullptr;
    }

    RT &getRT() {
        return rt;//return runtime configuration
    }

    void insertKey(uintptr_t tupleId) {
        //insert keys
        uint8_t arr[KP::maxKeySize()+8];
        rt.storage.loadKey(tupleId, arr);
        //the insert function may fail to insert in some very specific corner cases 
        //then, the insert process has to be repeated.
        while (!insert(tree, arr, tupleId, KP::maxKeySize()));
    }

    __attribute__((always_inline)) bool lookupVal(uintptr_t &val, K key, bool &isDuplicate) {
        //load key bytes
        uint8_t arr[KP::maxKeySize()+8];
        KP::keyToBytes(key, arr);
        //retrieve leaf val
        Node *leaf = lookup(tree, arr, KP::keySize(key), KP::maxKeySize());
        //check if it is a leaf
        if (isLeaf(leaf)) {
            //check for duplicates
            isDuplicate = hasDuplicate(leaf);
            //return leaf value
            val = getLeafValue(leaf);
            return true;
        }
        //not found
        return false;
    }

    //for range queries:
    // execute lambda f for every stored tuple id in the range [from,to) in order
    template<class F>
    void range(const K &from, const K &to, const F &f) {
        Node *end = findEnd(to);
        bool foundStart = false;
        bool foundEnd = false;

        uint8_t arr[KP::maxKeySize()+8];
        KP::keyToBytes(from, arr);

        range_(tree, end, arr, 0, foundStart, foundEnd, f);
    }

    size_t getSize() {
        return rt.statistics.getSize();
    }

    ~art_impl() {
        //cleanup tree
        cleanup(tree);
    }

    template<typename C>
    friend
    struct migrate_util;

    //for node measurements, internal access is required
    template<class A, class B>
    friend
    class measure_node;

    template<class A>
    friend
    class tuning;

};
