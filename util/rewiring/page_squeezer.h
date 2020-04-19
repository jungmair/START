#pragma once

#include <cstring>
#include <cinttypes>
#include <vector>
#include "../../nodes/node.tcc"
//this class tries to "squeeze" multiple virtual pages to one physical page
class page_squeezer {
    //stores for one page (identified by 8 bits) which of the 512 child pointer locations is already occupied
    typedef std::pair<uint8_t, std::bitset<512>> free_t;
    typedef std::pair<uint8_t, Node *> value_t;
    uint8_t mapping[128];

    std::vector<free_t> freeList;

    //helper method for rotating a bitset
    static void rotate(std::bitset<512> &b, unsigned m) {
        b = b << m | b >> (512 - m);
    }

    size_t pagenumber = 0;
public:
    page_squeezer() {
        //initialize mapping with 0xff -> page not mapped to any physical page
        memset(mapping, 0xff, sizeof(mapping));
        pagenumber = 0;
    }

    void squeeze(uint8_t prefix, const std::bitset<512> &required) {
        uint8_t page = prefix >> 1u;
        //iterate over all entries in free list
        for (free_t &free:freeList) {
            if ((free.second & required).none()) {
                //no overlaps between freelist entry and required
                //->squeeze pages 
                mapping[page] = free.first;
                //update freelist entry
                free.second = free.second | required;
                return;
            } else {
                //there are some overlaps, but maybe, we can still squeeze, putting some entries "one off"

                //calculate overlaps
                std::bitset<512> overlaps = (free.second & required);
                //calculate slots that are still free (and could be filled with overlaps)
                std::bitset<512> stillFree = free.second | required;
                if (overlaps[511])continue;//avoid jump to page start
                //rotate overlaps by one slot
                rotate(overlaps, 1); 
                if ((overlaps & stillFree).none()) {
                    //rotating the overlaps resolved them
                    mapping[page] = free.first;
                    free.second = stillFree | overlaps;
                    return;
                }
            }
        }
        //no squeezing was possible -> map page to itself -> one physical page will be allocated
        mapping[page] = page;
        pagenumber++;//update page number
        freeList.push_back({page, required});//add to freelist
    }

    const uint8_t *get_squeezed_mapping() const {
        return mapping;
    }

    size_t get_num_pages() const {
        return pagenumber;
    }

};
