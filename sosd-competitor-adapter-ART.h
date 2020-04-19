#pragma once


#include "../../util.h"
#include "config/config.tcc"
#include "art_impl.tcc"
#include <stdlib.h>    // malloc, free
#include <string.h>    // memset, memcpy
#include <stdint.h>    // integer types
#include <emmintrin.h> // x86 SSE intrinsics
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>  // gettime
#include <algorithm>   // std::random_shuffle
#include "config/storage/competitor_storage_provider.tcc"

template<class KeyType>
class ART_NEW : public Competitor {
    //using declarations to simplify code:
    static constexpr auto NodeTypeList = hana::make_tuple(hana::type_c<Node4>, hana::type_c<Node16>,
                                                          hana::type_c<Node48>, hana::type_c<Node256>);
    using NTL=decltype(NodeTypeList);
    using ARTConfig=ARTConfiguration<KeyType, NTL, competitor_storage_provider,no_statistics>;
    using RT=typename ARTConfig::RT_t;
    //pointer to art implementation
    art_impl<ARTConfig> *art;
    RT* config;
public:
    ART_NEW() : art(nullptr) {}

    void Build(const std::vector<KeyValue<KeyType>> &data) {
        //store reference to data
        data_ = &data;
        //create runtime config containing data
        config=new RT(data);
        //create art
        art = new art_impl<ARTConfig>(*config);
        //insert whole dataset
        for (size_t i = 0; i < data.size(); i++) {
            art->insertKey(i);
        }
    }
    //exclude some keys from inserting
    void BuildExcept(const std::vector<KeyValue<KeyType>> &data, std::vector<uint32_t> idx_for_later) {
        data_ = &data;
        RT config{data};
        art = new art_impl<ARTConfig>(config);
        size_t i_idx = 0;
        //insert almost all keys
        for (size_t i = 0; i < data.size(); i++) {
            if (i_idx < idx_for_later.size() && idx_for_later[i_idx] == i) {
                //ignore this key, as it is in the sorted list of excluded 
                i_idx++;
                continue;
            }
            art->insertKey(i);
        }
    }

    void insertLater(uint32_t idx) {
        art->insertKey(idx);
    }

    uint64_t EqualityLookup(const KeyType lookup_key) {
        //perform equality lookup
        uintptr_t val = 0;
        bool duplicate = false;
        //perform lookup
        if (!art->lookupVal(val, lookup_key, duplicate)) {
            util::fail("ART_NEW: search ended in inner node");
        }
        uint64_t entry_offset = val;
        if (!duplicate)return entry_offset;//direct hit -> done
        //else: handle duplicates:
        const std::vector<KeyValue<KeyType>> &data = *data_;
        uint64_t result = data[entry_offset].value;
        while (++entry_offset < data.size() && data[entry_offset].key == lookup_key) {
            result += data[entry_offset].value;
        }
        return result;
    }

    std::string name() const {
        return "ART_NEW";
    }

    std::size_t size() const {
        return art->getSize();
    }

    bool applicable(bool unique, const std::string &_data_filename) const { return true; }

    ~ART_NEW() {
        delete art;
        delete config;
    }

private:

    const std::vector<KeyValue<KeyType>> *data_;
};