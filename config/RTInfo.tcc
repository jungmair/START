#pragma once
#include "../util/rewiring/lkm_rewiring_provider_creator.hpp"
#include "../util/rewiring/mmap_rewiring_provider_creator.hpp"
//provides external state and configuration at runtime
template<typename K, typename NTL, template<typename> class STORAGE, class STATISTICS>
struct RTInfo{
    using storage_provider_t=STORAGE<K>;
    using storage_t=typename storage_provider_t::storage_t;

    //provides storage
    storage_provider_t storage;

    //for rewiring
    std::shared_ptr<rewiring_provider_creator> rewiring_creator;
    //statistics object
    STATISTICS statistics;

    RTInfo(storage_t &storage, bool use_lkm=false) : storage{storage},statistics{} {
        //depending on use_lkm: create rewiring_provider_creator
        if(use_lkm){
            rewiring_creator = std::shared_ptr<rewiring_provider_creator>((rewiring_provider_creator *) new lkm_rewiring_provider_creator);
        }else {
            rewiring_creator = std::shared_ptr<rewiring_provider_creator>((rewiring_provider_creator *) new mmap_rewiring_provider_creator);
        }
    }

};