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
#include "mmap_rewiring_provider.hpp"
#include "rewiring_provider_creator.h"

//factory for creating mmap-based rewiring providers
class mmap_rewiring_provider_creator :public rewiring_provider_creator{
    //file descriptor of the main memory file 
    int fd;
    //at which file offset do we start?
    size_t curr_offset=0;

public:
    mmap_rewiring_provider_creator() {

        //create file backed by RAM pages
        fd = memfd_create("ramfile", 0);
        //error handling
        if (fd == -1) {
            throw std::system_error(errno, std::generic_category(), "creation of ramfile failed");
        }
        if (ftruncate(fd, LONG_MAX) == -1) {
            throw std::system_error(errno, std::generic_category(), "ftruncate failed");
        }

    }
    std::shared_ptr<rewiring_provider> createProvider(size_t len){
        //handle a request for a new "rewiring area" of len pages

        //create rewiring provider at current offset
        rewiring_provider* ptr= new mmap_rewiring_provider(fd,curr_offset,len);
        std::shared_ptr<rewiring_provider> created(ptr);
        //increase offset -> next rewiring provider does not overlap
        curr_offset+=len;
        return created;
    }


    ~mmap_rewiring_provider_creator() {
        close(fd);
    }

};