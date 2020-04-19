#pragma once
/*
This file contains a general implementation of clear_cache and specializations for large multilevel nodes.
clear_cache allows for putting a node into a certain "cache state" i.e. cached / not cached, which is required for measuring node costs.

The only static method takes three arguments:
    static void clear(T *n, cache_status cs, std::vector<uint64_t> &contained_keys)
    
    1. n: Node pointer to the node under test that should be put into a defined caching state
    2. cs: then desired cache status (full_cached, header_cached, not_cached)
    3. contained_keys: vector of all keys that are stored in this node (might be useful for clearing the caches)

    The implementation of clear will then use the _mm_clflush instrinsic to flush all or selected cache lines of the node n

*/
template<class T>
struct clear_cache {
    static void clear(T *n, cache_status cs, std::vector<uint64_t> &/*contained_keys*/) {
        char *ptr = ((char *) n);
        switch (cs) {
            case full_cached:
                return;
            case header_cached:

                for (size_t i = 8 + 64; i < sizeof(T); i += 64) {
                    _mm_clflush(ptr + i);
                }
                return;
            case not_cached:
                for (size_t i = 0; i < sizeof(T); i += 64) {
                    _mm_clflush(ptr + i);
                }
                return;
        }

    }
};

template<>
struct clear_cache<Rewired64K> {
    static void clear(Rewired64K *n, cache_status cs, std::vector<uint64_t> &/*contained_keys*/) {
        switch (cs) {
            case full_cached:
                return;
            case header_cached:
                for (uint8_t page = 0; page < 128; page++) {
                    if (n->mapping[page] == page) {
                        for (int i = 0; i < 4096; i += 64) {
                            auto *ptr = (uint8_t *) n->array;
                            _mm_clflush(ptr + 4096 * page + i);
                        }
                    }
                }
                return;
            case not_cached:
                for (uint8_t page = 0; page < 128; page++) {
                    if (n->mapping[page] == page) {
                        for (int i = 0; i < 4096; i += 64) {
                            auto *ptr = (uint8_t *) n->array;
                            _mm_clflush(ptr + 4096 * page + i);
                        }
                    }
                }
                for (size_t i = 0; i < sizeof(Rewired64K); i += 64) {
                    _mm_clflush(((char *) n) + i);
                }
                return;

        }

    }
};

template<>
struct clear_cache<Rewired16M> {
    static void clear(Rewired16M *n2, cache_status cs, std::vector<uint64_t> &/*contained_keys*/) {
        switch (cs) {
            case full_cached:
                return;
            case header_cached:
                for (int prefix = 0; prefix <= 255; prefix++) {
                    if (n2->used[prefix]) {
                        Rewired64K *n = &n2->embedded_64k[prefix];
                        for (uint8_t page = 0; page < 128; page++) {
                            if (n->mapping[page] == page) {
                                for (int i = 0; i < 4096; i += 64) {
                                    auto *ptr = (uint8_t *) n->array;
                                    _mm_clflush(ptr + 4096 * page + i);
                                }
                            }
                        }
                    }
                }
                return;
            case not_cached:
                for (int prefix = 0; prefix <= 255; prefix++) {
                    if (n2->used[prefix]) {
                        Rewired64K *n = &n2->embedded_64k[prefix];
                        for (uint8_t page = 0; page < 128; page++) {
                            if (n->mapping[page] == page) {
                                for (int i = 0; i < 4096; i += 64) {
                                    auto *ptr = (uint8_t *) n->array;
                                    _mm_clflush(ptr + 4096 * page + i);
                                }
                            }
                        }
                    }
                }
                for (size_t i = 0; i < sizeof(Rewired16M); i += 64) {
                    _mm_clflush(((char *) n2) + i);
                }
                return;

        }
    }
};

template<>
struct clear_cache<Flat64K> {
    static void clear(Flat64K *n, cache_status cs, std::vector<uint64_t> &contained_keys) {
        switch (cs) {
            case full_cached:
                return;
            case header_cached:
                for (uint64_t x:contained_keys) {
                    uint8_t key[8];
                    key_props<uint64_t>::keyToBytes(x, key);
                    void *ptr = n->findChildPtr(&key[5]);
                    _mm_clflush(ptr);

                }
                return;
            case not_cached:
                for (uint64_t x:contained_keys) {
                    uint8_t key[8];
                    key_props<uint64_t>::keyToBytes(x, key);
                    void *ptr = n->findChildPtr(&key[5]);
                    _mm_clflush(ptr);

                }
                for (size_t i = 0; i < 128; i += 64) {
                    _mm_clflush(((char *) n) + i);
                }
        }

    }
};


template<>
struct clear_cache<Flat16M> {
    static void clear(Flat16M *n, cache_status cs, std::vector<uint64_t> &contained_keys) {
        switch (cs) {
            case full_cached:
                return;
            case header_cached:
                for (uint64_t x:contained_keys) {
                    uint8_t key[8];
                    key_props<uint64_t>::keyToBytes(x, key);
                    void *ptr = n->findChildPtr(&key[4]);
                    _mm_clflush(ptr);

                }
                return;
            case not_cached:
                for (uint64_t x:contained_keys) {
                    uint8_t key[8];
                    key_props<uint64_t>::keyToBytes(x, key);
                    void *ptr = n->findChildPtr(&key[4]);
                    _mm_clflush(ptr);

                }
                for (size_t i = 0; i < 128; i += 64) {
                    _mm_clflush(((char *) n) + i);
                }
        }

    }
};