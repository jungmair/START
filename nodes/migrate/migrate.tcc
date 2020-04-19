#pragma once
/**
 * responsible for migrations from one node type to another.
 * migrations can occur in two situations
 *  1. a node overflows -> entries must be migrated to next larger node type
 *  2. the tree is restructured
 */
#include "../../art_impl.tcc"
#include "../../util/compile-time-switch.tcc"
#include "../../config/node-props.tcc"
#include "../../leaf.tcc"
#include "migrate_util.tcc"
#include "rewired_node_util.tcc"

/**
 * default migration implementation for generic node types
 * From: the current node type
 * To: the node type to which the entries should be migrated
 */
template<class From, class To, class C>
struct migrate {
    using keyProps=typename C::key_props_t;

    static Node* apply(From *from, art_impl<C> &a, size_t depth, size_t levels) {
        if (from->levels == levels) {
            //case one: equal number of levels -> easy migration
            //create new node
            To *to = NodeProperties<To>::creator::create(levels);
            //iterate over old node and insert all entries into new one
            from->iterateOver([to](uint8_t *keyByte, Node *child) {
                to->insert(keyByte, child);
            });
            //also migrate prefix information
            to->movePrefixFrom(from);
            //log creation/destruction for statistic purposes
            a.getRT().statistics.registerNodeCreation(to);
            a.getRT().statistics.registerNodeDestruction(from);
            //destroy old node
            NodeProperties<From>::creator::destroy(from);
            //return new node
            return to;
        } else if (from->levels < levels) {
            //case two: new node spans over more level than the old one
            //create new node
            To *to = NodeProperties<To>::creator::create(levels);
            
            uint8_t arr[keyProps::maxKeySize()+8];
            std::vector<std::pair<uint8_t[8], Node *>> entries_if_failing;
            //collect all entries in a destructive way (i.e. the tree is modified)
            migrate_util<C>::visitAndClaim(from, arr, [&](uint8_t *arr, Node *n) {
                if (!entries_if_failing.empty()) {
                    //something went wrong, but we can not simply stop iteration
                    //-> only collect entries
                    std::pair<uint8_t[8], Node *> p;
                    std::memcpy(&p.first, arr, levels);
                    p.second = n;
                    entries_if_failing.push_back(p);
                    return;
                }
                //try to insert entry
                insert_result_t res = to->insert(arr, n);
                //node type signals selfgrow
                if (res == selfgrow) {
                    //try to selfgrow node
                    To *to2 = NodeProperties<To>::grow_props::selfgrow(to);
                    if (to2 == to) {
                        //selfgrow failed...
                        res = failed;
                    } else {
                        //selfgrow ok, try to insert current entry
                        to = to2;
                        res = to->insert(arr, n);
                    }
                }
                if (res == failed) {
                    //ups, something wrent wrong during migration
                    //collect already inserted entries
                    to->iterateOver([&](uint8_t *keyByte, Node *child) {
                        std::pair<uint8_t[8], Node *> p;
                        std::memcpy(&p.first, keyByte, levels);
                        p.second = child;
                        entries_if_failing.push_back(p);
                    });
                    //add the current entry causing the error to the collected entries
                    std::pair<uint8_t[8], Node *> p;
                    std::memcpy(&p.first, arr, levels);
                    p.second = n;
                    entries_if_failing.push_back(p);
                }
            }, depth, depth + levels, a, depth);
            if (!entries_if_failing.empty()) {
                //the migration failed!! -> switch back to "normal" ART nodes using the collected entries
                Node *alternativeNode = migrate_util<C>::createSubtree(entries_if_failing, levels, a);
                alternativeNode->movePrefixFrom(from);
                //destroy newly created node
                NodeProperties<To>::creator::destroy(to);
                return alternativeNode;
            }
            //finalize migration: migrate prefix, statistics, destroy old node
            to->movePrefixFrom(from);
            a.getRT().statistics.registerNodeCreation(to);
            a.getRT().statistics.registerNodeDestruction(from);
            NodeProperties<From>::creator::destroy(from);
            //return new node
            return to;
        }
        throw std::runtime_error("shrinking through migration is not implemented");
    }
};
//specialisation for creating a 2-level rewired node
template<class From, class C>
struct migrate<From, Rewired64K, C> {
    static Node* apply(From *from, art_impl<C> &a, size_t depth, size_t levels) {
        //use util function to create Rewired64K
        return rewired_node_util<C>::create64K(from,a,depth,levels,false);
    }
};
//specialisation for creating a 2-level rewired node
template<class From, class C>
struct migrate<From, Rewired16M, C> {
    using keyProps=typename C::key_props_t;
    using NT=decltype(C::nodeTypeList);

    static Node* apply(From *from, art_impl<C> &a, size_t depth, size_t /*levels*/) {
        //create rewired node
        Rewired16M *to = Rewired16M::create(a.getRT());
        //iterate over root of subtree that is replaced
        from->iterateOver([&](uint8_t *keyByte, Node *child) {
            if (isLeaf(child)) {
                //we have a leaf -> create Rewired64K for a single leaf (should not happen too often)
                //get key for leaf and store it in this format:           
                uint8_t arr2[keyProps::maxKeySize()];
                a.getRT().storage.loadKey(getLeafValue(child), arr2);
                //simulate more realistic case
                std::bitset<512> bs;
                bs[0] = 1;
                page_squeezer squeezer;
                //"calculate page layout", i.e. allocate a single page
                squeezer.squeeze(arr2[depth + 1], bs);
                //create a Rewired64K manually
                Rewired64K *converted = Rewired64K::createEmbedded(to->getEmbeddingInfo(arr2[depth]));
                //initialize  it with the page layout                                                 
                converted->initialize(squeezer);
                //insert the only entry
                converted->insert(&arr2[depth + 1], child);
                //update data for Rewired16M node
                to->count += converted->count;
                to->used[arr2[depth]] = true;
            } else if (child->type == Rewired64K::NodeType) {
                //child is already a Rewired64K node: good-> move it to the correct position
                auto mlNodeRewired2 = static_cast<Rewired64K *>(child);
                to->embed_existing_64K(keyByte[0], *mlNodeRewired2);
                a.getRT().statistics.registerNodeDestruction(mlNodeRewired2);
                delete mlNodeRewired2;
            } else {
                //depending on the current node type: create64K out of it
                compile_time_switch<NT>(child, [&](auto x) {
                    auto *converted = static_cast<Rewired64K *>(rewired_node_util<C>::create64K(
                            x, a, depth + 1, 2, true, to->getEmbeddingInfo(keyByte[0])));
                    //update numbers for Rewired16M node
                    to->count += converted->count;
                    to->used[keyByte[0]] = true;
                });
            }
        });
        //check, if size constraint is fullfilled
        if (to->getSize() / to->count > C::mem_constr) {
            //we need to much space -> deconstruct newly created rewired node -> switch back to normal ART nodes
            a.getRT().statistics.registerNodeCreation(to);
            //throw std::runtime_error("problem!");
            NodeProperties<From>::creator::destroy(from);
            return migrate_util<C>::backTo1LevelNodes(to,a,depth,3);
        }
        //finish: migrate prefix, statistics, return newly created node
        to->movePrefixFrom(from);
        a.getRT().statistics.registerNodeCreation(to);
        a.getRT().statistics.registerRewired16M(to->count, to->getSize());
        a.getRT().statistics.registerNodeDestruction(from);
        NodeProperties<From>::creator::destroy(from);
        return to;
    }
};

/**
 * specialisation for migrating from a node to a no node
 * this is triggerd automatically, if a node overflows, but no larger node type is known
 * solution: switch back to normal art
 */
template<class From, class C>
struct migrate<From, NoNode, C> {
    static Node* apply(From *from, art_impl<C> &a, size_t depth, size_t levels) {
        return migrate_util<C>::backTo1LevelNodes(from,a,depth,levels);
    }

};

//specialisation for migration Node4->Node16, taken from original ART implementation
template<class C>
struct migrate<Node4, Node16, C> {
    static Node16* apply(Node4 *from, art_impl<C> &, size_t, size_t) {
        auto *newNode = new Node16();
        newNode->count = 4;
        for (unsigned i = 0; i < 4; i++)
            newNode->getKey()[i] = flipSign(from->getKey()[i]);
        memcpy(newNode->getChild(), from->getChild(), from->count * sizeof(uintptr_t));
        newNode->movePrefixFrom(from);
        NodeProperties<Node4>::creator::destroy(from);
        return newNode;
    }
};

//specialisation for migration Node16->Node48, taken from original ART implementation
template<class C>
struct migrate<Node16, Node48, C> {
    static Node48* apply(Node16 *from, art_impl<C> &, size_t, size_t) {
        auto *newNode = new Node48();
        memcpy(newNode->getChild(), from->getChild(), from->count * sizeof(uintptr_t));
        for (unsigned i = 0; i < from->count; i++)
            newNode->getChildIndex()[flipSign(from->getKey()[i])] = i;
        newNode->count = from->count;
        newNode->movePrefixFrom(from);
        NodeProperties<Node16>::creator::destroy(from);
        return newNode;
    }
};
