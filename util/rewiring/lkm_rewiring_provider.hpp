#pragma once
#include<vector>
#include<unordered_set>
#include"rewiring_provider.hpp"
#include <sys/ioctl.h>
#include <cstring>
#include "communication.h"

typedef uint32_t PageId;

//implements rewiring using a kernel module (-> https://github.com/jungmair/rewiring-lkm)
class lkm_rewiring_provider: public rewiring_provider{
    //internal zero page 
    PageId zeroPage;
    //pageids of managed virtual memory
    PageId* pageids;
    
    //creates new page ids through the kernel module
    void createNewPageIds(size_t num,PageId* array){
        struct cmd createPageIds = {
                .type=CREATE_PAGE_IDS,
                .start=0,
                .len=num,
                .mapping_start=start,
                .payload=array,
        };
        ioctl(fd,REW_CMD,&createPageIds);
    }
    //writes page ids to kernel module -> perform rewiring
    virtual void syncToPT(size_t start,size_t len,PageId* pageIds){
        struct cmd setPagesCMD = {
                .type=SET_PAGE_IDS,
                .start=start,
                .len=len,
                .mapping_start=this->start,
                .payload=pageIds
        };
        ioctl(fd,REW_CMD,&setPagesCMD);
    }
public:
    lkm_rewiring_provider (int fd,size_t len,PageId zeroPage):rewiring_provider(fd,len),zeroPage(zeroPage){
        pageids=new PageId[len];
        //initialize with unassigned 
        std::wmemset(reinterpret_cast<wchar_t *>(pageids), PAGEID_UNASSIGNED, len);
    }
    
    //initializes a new rewiring
    virtual void init(const std::vector<std::pair<uint16_t,uint16_t>>& mappings){
        mmap(start,len*sizeof(Page),PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED,fd,0);
        //which distinct physical pages do we have to setup
        std::unordered_set<uint16_t> set;
        for(auto mapping:mappings){
            set.insert(mapping.second);
        }
        //allocate requested physical pages
        PageId* newPageIds=new PageId[set.size()];
        createNewPageIds(set.size(),newPageIds);
        //distribute newly allocated page ids
        size_t i=0;
        for(uint16_t a:set){
            pageids[a]=newPageIds[i++];
        }
        //set remaining values to the correct page ids
        PageId* all=new PageId[len];
        std::wmemset(reinterpret_cast<wchar_t *>(all), zeroPage, len);
        for(auto mapping:mappings){
            all[mapping.first]=pageids[mapping.second];
        }
        //write changes to kernel module
        syncToPT(0,len,all);
        //cleanup of temporary storage
        delete[] newPageIds;
        delete[] all;

    }
    //update a single page mapping
    virtual void rewire(Page *addr, size_t offset_pages){
        size_t off=addr-start;
        if(pageids[offset_pages]==PAGEID_UNASSIGNED){
            //if necessary: allocate new page
            createNewPageIds(1,&pageids[offset_pages]);
        }
        //write changes to kernel module
        syncToPT(off,1,&pageids[offset_pages]);
    }
    virtual ~lkm_rewiring_provider(){
        //cleanup page ids;
        delete[] pageids;
    }
};