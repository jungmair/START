#pragma once

#include <vector>
//simple storage provider, initialized with a simple vector of keys
//useful for testing, benchmarks, ...
template<typename K>
struct vec_storage_provider {
    using storage_t=std::vector<K>;
    storage_t& storage;
    vec_storage_provider(storage_t& storage):storage(storage){}

    inline void loadKey(uintptr_t tid, uint8_t key[]) {
        key_props<K>::keyToBytes(storage[tid], key);
    }
};
