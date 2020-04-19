#pragma once

#include <iostream>
#include <linux/memfd.h>
#include <sys/mman.h>
#include <unistd.h>
#include <map>
#include <algorithm>
#include <cassert>
#include <climits>
#include <fcntl.h>
#include "rewiring_provider.hpp"
#include "lkm_rewiring_provider.hpp"
#include "rewiring_provider_creator.h"

//factory for creating rewiring providers using a linux kernel module for rewiring
class lkm_rewiring_provider_creator :public rewiring_provider_creator{
    //file descriptor of the kernel module's device file
    int fd;
    //a page id representing a "zero" page
    uint32_t zeroPage;

public:
    lkm_rewiring_provider_creator() {
        //open file + handle errors
        fd= open("/dev/rewiring", O_RDWR);
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "opening of file failed");
        }
        //allocate "zero page" from kernel module through ioctl command
        struct cmd createPageIds = {
                .type=CREATE_PAGE_IDS,
                .start=0,
                .len=1,
                .mapping_start=NULL,
                .payload=&zeroPage,
        };
        ioctl(fd,REW_CMD,&createPageIds);

    }
    std::shared_ptr<rewiring_provider> createProvider(size_t len){
        return std::shared_ptr<rewiring_provider>(static_cast<rewiring_provider*>(new lkm_rewiring_provider(fd,len,zeroPage)));
    }


    ~lkm_rewiring_provider_creator() {
        close(fd);
    }

};