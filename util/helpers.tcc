#pragma once
template<typename int_t>
struct IntHelper {

};
//load up to 8 key bytes into a uint64_t
template<>
struct IntHelper<uint64_t> {
    template<size_t bytes>
    __attribute__((always_inline)) static constexpr uint64_t load(uint8_t *ptr) {
        //if number of bytes to load is known at compile time -> use this knowledge to allow for optimisation
        return load(ptr, bytes);
    }

    __attribute__((always_inline)) static constexpr uint64_t load(uint8_t *ptr, uint8_t bytes) {
        uint64_t swapped =
                (((uint64_t) ptr[0]) << 56ull) + (((uint64_t) ptr[1]) << 48ull) + (((uint64_t) ptr[2]) << 40ull) +
                (((uint64_t) ptr[3]) << 32ull) + (((uint64_t) ptr[4]) << 24ull) + (((uint64_t) ptr[5]) << 16ull) +
                (((uint64_t) ptr[6]) << 8ull) + (((uint64_t) ptr[7]));
        uint64_t mask = bytes == 8 ? ~0 : (0xffffffffffffffff << ((8 - bytes) * 8));
        return swapped & mask;
    }

    static void unload(uint8_t *dst, uint64_t src) {
        reinterpret_cast<uint64_t *>(dst)[0] = __builtin_bswap64(src);
    }
};
//load up to 4 key bytes into a uint32_t
template<>
struct IntHelper<uint32_t> {
    template<size_t bytes>
    __attribute__((always_inline)) static constexpr uint64_t load(uint8_t *ptr) {
        //if number of bytes to load is known at compile time -> use this knowledge to allow for optimisation
        return load(ptr, bytes);
    }

    __attribute__((always_inline)) static constexpr uint64_t load(uint8_t *ptr, uint8_t bytes) {
        uint32_t swapped = (((uint32_t) ptr[0]) << 24u) + (((uint32_t) ptr[1]) << 16u) + (((uint64_t) ptr[2]) << 8u) +
                           (((uint64_t) ptr[3]));
        uint32_t mask = bytes == 4 ? ~0 : (0xffffffff << ((4 - bytes) * 8));
        return swapped & mask;
    }

    static void unload(uint8_t *dst, uint32_t src) {
        reinterpret_cast<uint32_t *>(dst)[0] = __builtin_bswap32(src);
    }
};
//load up to 4 key bytes into a uint32_t
template<>
struct IntHelper<uint16_t> {
    template<size_t bytes>
    __attribute__((always_inline)) static constexpr uint64_t load(uint8_t *ptr) {
        //if number of bytes to load is known at compile time -> use this knowledge to allow for optimisation
        return load(ptr, bytes);
    }

    __attribute__((always_inline)) static constexpr uint64_t load(uint8_t *ptr, uint8_t /*bytes*/) {
        return ((uint32_t) ptr[0]) * 256 + (uint32_t) ptr[1];
    }

    static void unload(uint8_t *dst, uint16_t src) {
        reinterpret_cast<uint16_t *>(dst)[0] = __builtin_bswap16(src);
    }
};

//necessary for some SIMD instructions:
static constexpr uint8_t flipSign(uint8_t keyByte) {
    // Flip the sign bit, enables signed SSE comparison of unsigned values, used by Node16
    return keyByte ^ 128u;
}
