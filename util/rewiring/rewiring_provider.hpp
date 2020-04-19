#pragma once
#include<vector>
//helper definitions. Having a "Page" type allows elegant pointer arithmetics
constexpr size_t PAGE_SIZE_DEFAULT = 4096;
typedef struct {
    uint8_t data[PAGE_SIZE_DEFAULT];
} Page;

//abstract base class for rewiring providers
class rewiring_provider{
protected:
    //all rewiring providers require some file descriptor
    int fd;
    //virtual start address of area
    Page *start;
    //number of pages that this rewiring provider handles
    size_t len;

public:
    rewiring_provider (int fd,size_t len):fd(fd),start(nullptr),len(len){

    }
    void setStart(Page *start) {
        this->start = start;
    }
    Page* getStart(){
        return this->start;
    }
    //initializes a whole range of pages e.g. during creation of a new rewired node
    virtual void init(const std::vector<std::pair<uint16_t,uint16_t>>& mappings)=0;
    //performs a "simple" rewiring
    virtual void rewire(Page *addr, size_t offset_pages)=0;
    virtual ~rewiring_provider() = default;
};