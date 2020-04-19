#pragma once
/**
 * properties and toByte function for diffferent key types
 */
template<typename X>
struct key_props {
    static constexpr size_t maxKeySize();

    static inline void keyToBytes(const X &key, uint8_t bytes[]);

    static constexpr size_t keySize(X &key);
};

template<>
struct key_props<uint64_t> {
    __attribute__((always_inline)) static constexpr size_t maxKeySize() {
        return 8;
    }

    __attribute__((always_inline)) static size_t keySize(uint64_t &/*key*/) {
        return 8;
    }

    __attribute__((always_inline)) static inline void keyToBytes(const uint64_t &okey, uint8_t bytes[]) {
        reinterpret_cast<uint64_t *>(bytes)[0] = __builtin_bswap64(okey);
    }
};

template<>
struct key_props<uint32_t> {
    __attribute__((always_inline)) static constexpr size_t maxKeySize() {
        return 4;
    }

    __attribute__((always_inline)) static size_t keySize(uint32_t &/*key*/) {
        return 4;
    }

    __attribute__((always_inline)) static inline void keyToBytes(const uint32_t &okey, uint8_t bytes[]) {
        reinterpret_cast<uint32_t *>(bytes)[0] = __builtin_bswap32(okey);
    }
};

template<>
struct key_props<std::string> {
    static constexpr size_t maxKeySize() {
        return 1024;
    }

    static size_t keySize(std::string &key) {
        return key.length();
    }

    static inline void keyToBytes(const std::string &okey, uint8_t bytes[]) {
        //simple memcpy is enough
        memcpy(bytes, okey.c_str(), okey.length());
        std::memset(bytes + okey.length(), 0, 8);
        bytes[okey.length()] = 0;
    }
};
