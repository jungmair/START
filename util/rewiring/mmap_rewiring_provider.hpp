#pragma once
#include<vector>
#include"rewiring_provider.hpp"

//provides mmap-based rewiring
class mmap_rewiring_provider: public rewiring_provider{
    //at which offset in the main memory file does this provider start?
    size_t off;

    void rewire_(Page *addr, size_t offset_pages, size_t n_pages = 1) {
        //perform a single rewiring operation, map the phyiscal pages "[offset_pages,offset_pages+n_pages)" to virtual address addr

        if (n_pages == 0) {
            return; //rewiring of length 0 is not useful
        }
        //perform rewiring with mmap
        void *res = mmap(addr, sizeof(Page) * n_pages, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd,
                            offset_pages * sizeof(Page));
        //check for problems
        if (res == MAP_FAILED) {
            throw std::system_error(errno, std::generic_category(), "mmap failed");
        }
        //might in some cases improve page fault behavior
        madvise(res, n_pages * sizeof(Page), MADV_WILLNEED);
    }
public:
    mmap_rewiring_provider (int fd,size_t curr_offset,size_t len):rewiring_provider(fd,len),off(curr_offset){

    }
    
    virtual void init(const std::vector<std::pair<uint16_t,uint16_t>>& mappings){
        //initialize -> sequence of simple rewirings
        for(auto mapping:mappings){
            rewire_(start+mapping.first, off + mapping.second, 1);
        }
    }
    virtual void rewire(Page *addr, size_t offset_pages){
        //single rewiring
        rewire_(addr, off + offset_pages, 1);
    }
};