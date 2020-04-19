#pragma once

#include <sys/mman.h>
#include <fcntl.h>
#include <system_error>



class reservation{
    void* start;
    size_t reserved_length;

    static int open_dev_zero() {
        int fd_zero = open("/dev/zero", O_RDONLY);
        if (fd_zero == -1) {
            throw std::system_error(errno, std::generic_category(), "open(/dev/zero) failed");
        }
        return fd_zero;
    }
    static int get_zero_fd(){
        static int fd_zero = open_dev_zero();
        return fd_zero;
    }
public:
    reservation():reserved_length(0){}
    reservation(size_t length):reserved_length(length){
        if(length){
            start = mmap(NULL, length, PROT_READ, MAP_SHARED, get_zero_fd(), 0);
            if (start == MAP_FAILED) {
                throw std::system_error(errno, std::generic_category(), "mmap failed");
            }
        }
    }
    reservation &operator=(reservation &&res){
        if (reserved_length) {
            if(munmap(start, reserved_length)!=0){
                throw std::system_error(errno, std::generic_category(), "munmap failed");
            }
        }
        start = res.start;
        reserved_length = res.reserved_length;
        res.reserved_length = 0;
        return *this;
    }
    void* getStart(){
        return start;
    }
    size_t getReservedLength(){
        return reserved_length;
    }
    ~reservation() noexcept(false){
        if (reserved_length) {
            if(munmap(start, reserved_length)!=0){
                throw std::system_error(errno, std::generic_category(), "munmap failed");
            }
        }
    }
};
