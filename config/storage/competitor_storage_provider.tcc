#pragma once
//storage provider that is initialized with a vector of KeyValues
//useful, for plugging START into the SOSD benchmark
template<typename K>
struct competitor_storage_provider {
    using storage_t=const std::vector <KeyValue<K>>;
    storage_t& storage;
    competitor_storage_provider(storage_t& storage):storage(storage){}
    inline void loadKey(uintptr_t tid, uint8_t key[]) {
        key_props<K>::keyToBytes(storage[tid].key, key);
    }
};