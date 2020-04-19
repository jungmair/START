#pragma once

#include<vector>
#include<bitset>
#include<list>
#include<functional>
#include<memory>
#include "../../util/rewiring/rewiring_provider.hpp"
#include "../../util/rewiring/page_squeezer.h"
#include "../../util/helpers.tcc"
#include "../../util/rewiring/reservation.h"

//rewired node storing up to 2^16 child pointers
class Rewired64K : public Node {
public:
    //required informations to embed a Rewiring64K into a Rewiring16M
    struct embedding_info{
        Rewired64K* embeddedLocation;
        std::shared_ptr<rewiring_provider> rewiringProvider;
        embedding_info():embeddedLocation(nullptr),rewiringProvider(){}
    };

    class Rewired64KIterator {
        //rewired64k node that is iterated
        Rewired64K *node;
        //current position in node
        size_t pos;
    public:
        Rewired64KIterator() : Rewired64KIterator(nullptr, 0) {}

        Rewired64KIterator(Rewired64K *node, size_t pos) : node(node), pos(pos) {}

        Rewired64KIterator &operator++() {
            //always increment by one
            pos++;
            //skip all following empty slots
            for (; pos < 1 << 16; pos++) {
                //current virtual page:
                uint8_t page = pos >> 9;
                if (node->getPageOffFromTagged(pos) == 0 && node->mapping[page] == 0xff) {
                    //currently switched page and whole page can be skipped
                    pos += 511;
                    continue;
                } else if (node->getPageFromTagged(node->array[pos]) == page && node->array[pos] != 0) {
                    //finally we found a non-empty slot belonging to the current page -> return
                    return *this;
                }

            }
            return *this;
        }

        Node *operator*() const {
            //get child pointer at current location (untag)
            return reinterpret_cast<Node *>(node->array[pos] >> 16);
        }

        bool operator==(const Rewired64KIterator &other) {
            //compare iterators by node as well as position
            return other.node == this->node && other.pos == this->pos;
        }

        bool operator!=(const Rewired64KIterator &other) {
            return !(*this == other);
        }
    };
    using iterator_t=Rewired64KIterator;

    static constexpr uint8_t NodeType = 4;

private:
    uint64_t *array;
    typedef std::pair<uint8_t, Node *> value_t;
    std::shared_ptr<rewiring_provider> rewiredAccess;
    size_t num_pages;
    uint8_t mapping[128];
    reservation res;
    __attribute__((always_inline)) static constexpr bool tagIsEqual(uint64_t a,uint64_t b){
        //helper function for checking if the tag is equal
        return (a & 0xffff) == (b & 0xffff);
    }
    __attribute__((always_inline)) uint64_t getTagged(uint8_t *key, Node *child) {
        uint64_t ptrVal = reinterpret_cast<uint64_t>(child);
        return (ptrVal << 16) | IntHelper<uint16_t>::template load<2>(key);
    }


    uint8_t getPageFromTagged(uint64_t tagged) {
        return (tagged >> 9) & 0x7f;
    }

    uint16_t getPageOffFromTagged(uint64_t tagged) {
        return tagged & 0x1ff;
    }

    void extract_page_entries(uint8_t page_to_extract, std::vector<uint64_t> &extracted) {
        //calculate start of page 
        size_t start = page_to_extract << 9;
        //iterate over page
        for (size_t i = start; i < start + 512; i++) {
            if (array[i] != 0 && getPageFromTagged(array[i]) == page_to_extract) {
                //entry is populated and matches the page 
                //add entry to extracted
                extracted.push_back(array[i]);
                //clear slot
                array[i] = 0;
            }
            size_t page_offset = i - start;
            if (array[i] == 0 && page_offset < 511 && getPageOffFromTagged(array[i + 1]) == page_offset &&
                getPageFromTagged(array[i + 1]) != page_to_extract) {
                //the current slot is empty, and the next slot contains a "off-by-one" entry
                //->move entry by one to the original slot
                array[i] = array[i + 1];
                //clear next slot
                array[i + 1] = 0;
            }
        }
    }

    void unshare(uint8_t vPage) {
        std::vector<uint64_t> extracted;
        //extract matching entries
        extract_page_entries(vPage, extracted);
        if (mapping[vPage] == vPage) {
            //we need more space, but we share our space with others
            size_t newSharer = 0xff;
            //iterate over all virtual pages that share the current page
            for (int i = 0; i < 128; i++) {
                if (mapping[i] == vPage && i != vPage) {
                    if (newSharer == 0xff) {
                        //take the first other virtual page that shares the page and make this page to the new "sharer"
                        newSharer = i;
                        mapping[i] = newSharer;
                        rewiredAccess->rewire(rewiredAccess->getStart() + i, newSharer);
                        //copy shared content and clear previously shared
                        std::memcpy(rewiredAccess->getStart() + i, rewiredAccess->getStart() + vPage, 4096);
                        std::memset(rewiredAccess->getStart() + vPage, 0, 4096);
                        //update num_pages
                        num_pages++;
                    } else {
                        //update mapping + rewire
                        mapping[i] = newSharer;
                        rewiredAccess->rewire(rewiredAccess->getStart() + i, newSharer);
                    }
                }
            }
        }
        //update mapping
        mapping[vPage] = vPage;
        //rewire such that the virtual page points to a new physical page
        rewiredAccess->rewire(rewiredAccess->getStart() + vPage, vPage);
        //insert extracted entries into new physical page
        for (uint64_t entry:extracted) {
            size_t offset = entry & 0xffff;
            array[offset] = entry;
        }
    }

public:
    Rewired64K():Node(NodeType, 2){}

    Rewired64K(std::shared_ptr<rewiring_provider> rewiringProvider) : Node(NodeType, 2),
                                                                                        rewiredAccess(rewiringProvider),
                                                                                        num_pages(0),res(0) {
        if(rewiringProvider->getStart()==nullptr){
            //rewiringProvider does not already bring reserved virtual memory
            //->reserve and set 
            res=reservation(128*4096);                                                                                  
            rewiredAccess->setStart((Page*)res.getStart());
        }                                                                                   
        array = (uint64_t *) rewiredAccess->getStart();
        count = 0;
    }

    template<typename C>
    static Rewired64K *create(C &config) {
        return new Rewired64K(config.rewiring_creator->createProvider(128));
    }

    static Rewired64K *createEmbedded(embedding_info embedding) {
        return new(embedding.embeddedLocation) Rewired64K(embedding.rewiringProvider);
    }

    void initialize(page_squeezer &squeezer) {
        //copy squeezed mapping
        memcpy(mapping, squeezer.get_squeezed_mapping(), 128);
        //set num pages
        num_pages = squeezer.get_num_pages();
        //"reformat" mapping
        std::vector<std::pair<uint16_t,uint16_t>> mapping_vector;
        for (int i = 0; i < 128; i++) {
            if (mapping[i] != 0xff) {
                mapping_vector.emplace_back(i,mapping[i]);
            }
        }
        //perform rewiring
        rewiredAccess->init(mapping_vector);
    }

    size_t getSize() {
        //calculate total size of node
        size_t size = sizeof(Rewired64K);
        size += num_pages * 4096;
        return size;
    }

    void move(Page *newAddr) {
        //moves the rewired storage of this node to another virtual address
        //update start of rewiring provider
        rewiredAccess->setStart(newAddr);
        //update array pointer
        array = (uint64_t *) newAddr;
        //rewire pages
        for (int i = 0; i < 128; i++) {
            if (mapping[i] != 0xff) {
                rewiredAccess->rewire(newAddr + i, mapping[i]);
                //prefault page if not done automatically by rewiring implementation
                char* ptr=(char*)(newAddr+i);
                //do not optimize
                ((char volatile *)(ptr))[0] = ptr[0];
            }
        }
    }

    iterator_t begin() {
        uint8_t zero[2] = {0, 0};
        return first_geq(zero);
    }

    iterator_t end() {
        return Rewired64KIterator(this, 1 << 16);
    }

    iterator_t first_geq(uint8_t *key) {
        size_t offset = IntHelper<uint16_t>::load<2>(key);
        for (unsigned i = offset; i < 1 << 16; i++) {
            uint8_t curr_page = i >> 9;
            if (getPageOffFromTagged(i) == 0 && mapping[curr_page] == 0xff) {
                    i += 511;
            }else if (getPageFromTagged(array[i]) == curr_page && (array[i] & 0xffff) >= offset && array[i] != 0) {
                    return Rewired64KIterator(this, i);
            }
        }
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        //calculate offset from key bytes
        size_t original_offset = ((uint32_t) key[0]) * 256 + (uint32_t) key[1];
        size_t offset = original_offset;
        if (tagIsEqual(array[offset],original_offset)) {
            //tag matched -> return pointer to child pointer
            return (Node **) &array[offset];
        }
        //no entry found at expected offset, try "off-by-one"
        offset++;
        if (offset % 512 == 0)return NULL;//do not jump to page start
        if (tagIsEqual(array[offset],original_offset)) {
            //tag matched -> return pointer to child pointer
            return (Node **) &array[offset];
        }
        return NULL;
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *key) {
        //calculate offset from key bytes
        size_t original_offset=((uint32_t) key[0]) * 256 + (uint32_t) key[1];
        size_t offset = original_offset;
        if (tagIsEqual(array[offset],original_offset)) {
            //tag matched -> return child pointer (untag by shifting)
            return (Node *) (array[offset] >> 16);
        }
        //no entry found at expected offset, try "off-by-one"
        offset++;
        if (offset % 512 == 0)return NULL;
        if (tagIsEqual(array[offset],original_offset)) {
            //tag matched -> return child pointer (untag by shifting)
            return (Node *) (array[offset] >> 16);
        }
        return NULL;
    }

    __attribute__((always_inline)) insert_result_t insert(uint8_t *key, Node *child) {
        this->count++;
        size_t offset = ((uint32_t) key[0]) * 256 + (uint32_t) key[1];
        uint8_t page = key[0] >> 1;
        if (mapping[page] != 0xff) {
            //responsible page is already mapped
            if (!array[offset]) {
                //calculated slot is free -> insert directly
                array[offset] = getTagged(key, child);
                return success;
            } else if ((offset + 1) % 512 != 0 && !array[offset + 1]) {
                //"off-by-one" slot is free -> insert directly
                array[offset + 1] = getTagged(key, child);
                return success;
            }
            //can't insert directly -> perform unshare operation
            unshare(page);
            //now insert is possible
            array[offset] = getTagged(key, child);
        } else {
            //related page is not previously mapped -> use new page 
            mapping[page] = page;
            rewiredAccess->rewire(rewiredAccess->getStart() + page, page);
            array[offset] = getTagged(key, child);
        }
        return success;
    }

    template<typename F>
    void iterateOver(F f) {
        //iterate over all virtual pages
        for (uint8_t page = 0; page < 128; page++) {
            if (mapping[page] != 0xff) {
                //virtual page is not zero -> iterate over page
                for (size_t pos = page * 512; pos < (size_t)(page + 1) * 512; pos++) {
                    uint64_t val = array[pos];
                    if (val && page == getPageFromTagged(val)) {
                        //valid entry and tag matches current virtual page
                        uint8_t keyBytes[] = {(uint8_t)((val>>8) & 0xff), (uint8_t) (val & 0xff)};
                        Node *ptr = (Node *) (val>>16);
                        f(keyBytes, ptr);
                    }
                }
            }
        }
    }

    //allow access to clear_cache struct for measurement
    template<class T>
    friend
    struct clear_cache;
};

