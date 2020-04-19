#pragma once
#include <cassert>
//contains utility functions for migrating entries from one node type to another
template<typename Config>
struct migrate_util {
    using keyProps=typename Config::key_props_t;
    using NT=decltype(Config::nodeTypeList);
    using key_ptr=std::pair<uint8_t[8], Node *>;

    static void reducePrefix(Node *ptr, size_t reduceBy, uint8_t *arr, art_impl<Config> &a, size_t depth) {
        assert(reduceBy <= 8);
        if (ptr->prefixLength == reduceBy) {
            //prefix is completely removed -> simple
            std::memcpy(arr, ptr->prefix, reduceBy);
            ptr->prefixLength = 0;
        } else {
            //handle also cases when prefix is not stored completely in node 
            uint8_t tmpPrefix[8];
            std::memcpy(tmpPrefix, ptr->prefix, 8);
            uint8_t fullKey[keyProps::maxKeySize()];
            if (ptr->prefixLength > 8) {
                a.loadFullKey(ptr, fullKey);
            }
            uint8_t *prefixPtr = ptr->prefixLength > 8 ? &fullKey[depth] : tmpPrefix;
            std::memcpy(arr, prefixPtr, reduceBy);

            //set reduced prefix
            size_t newPrefixLen = ptr->prefixLength - reduceBy;
            std::memcpy(ptr->prefix, &prefixPtr[reduceBy], std::min(newPrefixLen, 8ul));
            ptr->prefixLength = newPrefixLen;
        }
    }

    template<class F>
    static void visitAndClaim(Node *ptr, uint8_t *arr, F f, size_t depth, size_t target_depth, art_impl<Config> &a,
                              size_t start_depth, bool usePrefix = false) {
        //visit all child pointers located between start_depth and target_depth
        //additionally: destroy nodes and adapt prefixes-> prepare for mulit-level nodes
        if (ptr == nullptr)return;//nullptr-> nothing to do
        if (depth == target_depth) {//we are at target_depth:visit
            f(&arr[start_depth], ptr);
            return;
        }
        if (isLeaf(ptr)) {
            //leaf detected
            //-> load key from storage
            uint8_t arr2[keyProps::maxKeySize()];
            a.rt.storage.loadKey(getLeafValue(ptr), arr2);
            //copy missing key bytes in order to reach target_depth
            std::memcpy(arr + depth, &arr2[depth], target_depth - depth);
            //directly jump to target_depth
            visitAndClaim(ptr, arr, f, target_depth, target_depth, a, start_depth);

        } else {
            //inner node detected
            //how much levels are still missing
            size_t diff = target_depth - depth;
            //handle prefix if necessary
            if (ptr->prefixLength && (start_depth != depth || usePrefix)) {
                //how many prefix bytes are used
                size_t prefixConsidered = std::min(diff, (size_t) ptr->prefixLength);
                std::memcpy(&arr[depth], ptr->prefix, prefixConsidered);
                if (ptr->prefixLength >= diff) {
                    //node does still exist, as prefix is enough to reach target_depth
                    //-> reduce prefix of node
                    reducePrefix(ptr, diff, arr + depth, a, depth);
                    //recursively handle node
                    visitAndClaim(ptr, arr, f, target_depth, target_depth, a, start_depth);
                    return;
                }
                depth += ptr->prefixLength;
            }
            compile_time_switch<NT>(ptr, [&](auto x) {
                //iterate over entries and handle them recursively
                x->iterateOver([&](uint8_t *keyByte, Node *child) {
                    std::memcpy(&arr[depth], keyByte, x->levels);
                    visitAndClaim(child, arr, f, depth + x->levels, target_depth, a, start_depth);
                });
                if (start_depth != depth) {
                    using localNodeType=typename std::remove_pointer<decltype(x)>::type;
                    //cleanup, but do not delete the start node, will be cleaned up later
                    a.rt.statistics.registerNodeDestruction(x);
                    NodeProperties<localNodeType>::creator::destroy(x);
                }
            });
        }
    }

    static Node *create1LevelNode(std::vector<std::pair<uint8_t, Node *>> &entries, art_impl<Config> &a) {
        Node *res = nullptr;
        //search for a node type that supports 1 level and the required number of children
        boost::hana::for_each(Config::nodeTypeList, [&](const auto x) {
            using replaceType=typename decltype(x)::type;
            //"break"
            if (res != nullptr)return;
            //is this a 1-level node?
            if (!NodeProperties<replaceType>().levelSizeProps.supports_levels(1)) {
                return;
            }
            //check if supports required number of children
            if (! NodeProperties<replaceType>().levelSizeProps.supports_children(1, entries.size())) {
                return;
            }
            //create matching node
            replaceType *t = NodeProperties<replaceType>::creator::create(1);
            //register node creation
            a.getRT().statistics.registerNodeCreation(t);

            //insert all entries
            for (auto entry:entries) {
                t->insert(&entry.first, entry.second);
            }
            res = t;

        });
        return res;
    }
    //recursively creates a subtree consisting of level-1 nodes from a sorted lists of pairs:(key bytes, child pointers)
    static Node *
    createSubtree(std::vector<key_ptr> &entries, size_t depth, size_t target_depth, size_t lb,
                  size_t ub, art_impl<Config> &a) {
        //current task: return a Node* representing the subtree containing all pairs in the range [lb,ub)
        if (target_depth == depth) {
            //only one pair left -> recursion base case -> return pointer
            assert(lb + 1 == ub);
            return entries[lb].second;
        }
        //multiple pairs available
        size_t local_lb = lb;
        size_t local_ub = 0;
        std::vector<std::pair<uint8_t, Node *>> local_children;
        //iterate over all pairs in range and split them into groups according to the current key byte
        for (size_t i = lb; i < ub - 1; i++) {
            if (entries[i].first[depth] != entries[i + 1].first[depth]) {
                local_ub = i + 1;
                //for each group: recursively create a subtree for this group and collect the results in local_chidren
                local_children.push_back(
                        {entries[i].first[depth], createSubtree(entries, depth + 1, target_depth, local_lb, local_ub, a)});
                local_lb = local_ub;
            }
        }
        local_ub = ub;
        //also create a subtree for the last group
        local_children.push_back(
                {entries[ub - 1].first[depth], createSubtree(entries, depth + 1, target_depth, local_lb, local_ub, a)});
        //now, we have to handle a list of subtree roots and return only one node pointer
        if (local_children.size() == 1) {
            //only one child 
            uint8_t keyByte = local_children[0].first;
            Node *n = local_children[0].second;
            if (isLeaf(n)) {
                //only child is leaf -> return leaf, representing subtree
                return n;
            } else {
                //only child is a "real" node -> use path compression -> add current key byte to node prefix
                std::memmove(n->prefix + 1, n->prefix, std::min(n->prefixLength, (uint16_t) 7));
                n->prefix[0] = keyByte;
                n->prefixLength++;
                //then return node
                return n;
            }
        } else {
            //create a 1 level nodes for all collected children and return it
            return create1LevelNode(local_children,a);
        }

    }

    static Node *createSubtree(std::vector<key_ptr> &entries, size_t levels, art_impl<Config> &a) {
        return createSubtree(entries, 0, levels, 0, entries.size(), a);
    }
    template<class From>
    static Node *backTo1LevelNodes(From *from, art_impl<Config> &a, size_t /*depth*/, size_t levels) {
        //collect all entries 
        std::vector<std::pair<uint8_t[8], Node *>> vec;
        from->iterateOver([&](uint8_t *keyByte, Node *child) {
            std::pair<uint8_t[8], Node *> p;
            std::memcpy(&p.first, keyByte, levels);
            p.second = child;
            vec.push_back(p);
        });
        //create subtree from collected entries
        Node *alternativeNode = createSubtree(vec, levels, a);
        //finalize: migrate prefix, statistics, delete old node
        alternativeNode->movePrefixFrom(from);
        a.rt.statistics.registerNodeDestruction(from);
        NodeProperties<From>::creator::destroy(from);
        //return root of newly created subtree
        return alternativeNode;
    } 
};