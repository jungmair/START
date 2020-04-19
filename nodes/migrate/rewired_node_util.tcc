#pragma once
#include "migrate_util.tcc"

template<typename Config>
struct rewired_node_util {
    using keyProps=typename Config::key_props_t;
    using RT=typename Config::RT_t;
    using NT=decltype(Config::nodeTypeList);
    using key_ptr=std::pair<uint8_t[8], Node *>;

    static void computeSqueezedMapping(Node *n,  RT& rt, size_t depth, page_squeezer *squeezer) {
        if (isLeaf(n)) {
            throw std::runtime_error("leaf is not supported");
        }
        compile_time_switch<NT>(n, [&](auto x) {
            //each page has 512 child pointer slots
            //-> represent slot status of current page
            std::bitset<512> occupied;
            //512 slots per page-> page will be filled from multiple nodes
            //store page that was handled in the last iteration
            uint8_t last_page = 0;
            //we have to delay the squeezing operation, until the page is done
            bool delayed = false;
            x->iterateOver([&](uint8_t *keyByte, Node *child) {
                //get current page by shifting
                uint8_t current_page = keyByte[0] >> 1;
                //basis offset on page
                size_t offset = (keyByte[0] & 1) ? 256 : 0;

                if (delayed&&last_page != current_page) {
                    //squeezing is delayed but we switch page -> can't delay anymore -> squeeze
                    squeezer->squeeze(last_page << 1, occupied);
                    //reset page-based occupied bitset + delayed flag
                    occupied.reset();
                    delayed = false;
                }
                if (isLeaf(child)) {
                    //handle leaf -> load full key and mark the corresponding location as occupied
                    uint8_t arr2[keyProps::maxKeySize()];
                    rt.storage.loadKey(getLeafValue(child), arr2);
                    occupied[offset + arr2[depth + 1]] = true;
                } else {
                    //handle node
                    compile_time_switch<NT>(child, [&](auto y) {
                        y->iterateOver([&](uint8_t *keyByte2, Node *) {
                            //iterate over all entries and mark the corresponding locations as occupied
                            occupied[offset + keyByte2[0]] = true;
                        });
                    });
                }
                //update last page + always set delayed
                last_page = current_page;
                delayed = true;
            });
            if (delayed) {
                //there is still something delayed to squeeze
                squeezer->squeeze(last_page << 1, occupied);
            }
        });
    }
    template<class From>
    static Node *create64K(From *from, art_impl<Config> &a, size_t depth, size_t levels, bool embedded,Rewired64K::embedding_info eInfo=Rewired64K::embedding_info()) {
        uint8_t arr[keyProps::maxKeySize()];
        Rewired64K *to = nullptr;
        if (!embedded) {
            //create standalone Rewired64K
            to = Rewired64K::create(a.getRT());
        } else {
            //create embedded Rewired64K for Rewired16M
            to = Rewired64K::createEmbedded(eInfo);
        }
        //initially squeeze pages
        page_squeezer squeezer;
        rewired_node_util<Config>::computeSqueezedMapping(from, a.getRT(), depth, &squeezer);
        //and initialize rewiring
        to->initialize(squeezer);

        size_t prefixLen = from->prefixLength;
        //migrate entries from old nodes to rewired64k
        migrate_util<Config>::visitAndClaim(from, arr, [&](uint8_t *arr, Node *n) {
            to->insert(arr, n);//simple inserts
        }, depth, depth + 2, a, depth, embedded);
        if (!embedded || prefixLen == 0) {
            //destroy original node only when this is not done elsewhere
            to->movePrefixFrom(from);
            a.getRT().statistics.registerNodeDestruction(from);
            NodeProperties<From>::creator::destroy(from);
        }
        if (!embedded && to->getSize() / to->count > Config::mem_constr) {
            //not embedded and memory constraints are violated -> destroy rewired node
            //register node creation as backTo1LlevelNodes will register a destruction
            a.getRT().statistics.registerNodeCreation(to);
            return migrate_util<Config>::backTo1LevelNodes(to,a,depth,levels);
        }
        if (!embedded) {
            //do statistics, if not embedded
            a.getRT().statistics.registerNodeCreation(to);
            a.getRT().statistics.registerRewired64K(to->count, to->getSize());
        }
        return to;
    }
};