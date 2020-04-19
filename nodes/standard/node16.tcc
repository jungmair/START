#pragma once

#include <emmintrin.h>
#include "../../util/helpers.tcc"

/**
 * Node16: standard implementation: array of 16 key bytes + array of up to 16 child pointers: use 128-bit SIMD instructions
 */
class Node16 : public Node {
    uint8_t key[16];
    Node *child[16];
public:
    using iterator_t=Node **;
    static constexpr uint8_t NodeType = 1;

public:
    Node16() : Node(NodeType, 1) {
        memset(child, 0, sizeof(child));
    }

    Node16(size_t /*levels*/) : Node16() {

    }

    iterator_t begin() {
        return &child[0];
    }

    iterator_t end() {
        return &child[count];
    }

    iterator_t first_geq(uint8_t *key) {
        uint8_t keyByteFlipped = flipSign(key[0]);
        __m128i cmp = _mm_cmplt_epi8(_mm_set1_epi8(keyByteFlipped),
                                     _mm_loadu_si128(reinterpret_cast<__m128i *>(this->key)));
        uint16_t bitfield = _mm_movemask_epi8(cmp) & (0xFFFFu >> (16 - this->count));
        unsigned pos = bitfield ? __builtin_ctz(bitfield) : this->count;
        if (pos > 0 && this->key[pos - 1] == keyByteFlipped) {
            return &child[pos - 1];
        }
        return &child[pos];
    }

    Node **findChildPtr(uint8_t *keyBytes) {
        __m128i cmp = _mm_cmpeq_epi8(_mm_set1_epi8(flipSign(keyBytes[0])),
                                     _mm_loadu_si128(reinterpret_cast<__m128i *>(this->key)));
        uint16_t bitfield = _mm_movemask_epi8(cmp) & (0xFFFFu >> (16 - this->count));
        if (bitfield)
            return &this->child[__builtin_ctz(bitfield)];
        else {
            return nullptr;
        }
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *keyByte) {
        __m128i cmp = _mm_cmpeq_epi8(_mm_set1_epi8(flipSign(*keyByte)),
                                     _mm_loadu_si128(reinterpret_cast<__m128i *>(this->key)));
        auto bitfield = _mm_movemask_epi8(cmp) & ((1u << this->count) - 1);
        if (bitfield) {
            return this->child[__builtin_ctz(bitfield)];
        } else
            return nullptr;
    }

    insert_result_t insert(uint8_t *keyBytes, Node *newChild) {
        if (this->count < 16) {
            uint8_t keyByteFlipped = flipSign(keyBytes[0]);
            __m128i cmp = _mm_cmplt_epi8(_mm_set1_epi8(keyByteFlipped),
                                         _mm_loadu_si128(reinterpret_cast<__m128i *>(this->key)));
            uint16_t bitfield = _mm_movemask_epi8(cmp) & (0xFFFFu >> (16 - this->count));
            unsigned pos = bitfield ? __builtin_ctz(bitfield) : this->count;
            memmove(this->key + pos + 1, this->key + pos, this->count - pos);
            memmove(this->child + pos + 1, this->child + pos, (this->count - pos) * sizeof(uintptr_t));
            this->key[pos] = keyByteFlipped;
            this->child[pos] = newChild;
            this->count++;
            return success;
        }
        return failed;
    }

    template<typename F>
    void iterateOver(F f) {
        for (uint32_t i = 0; i < this->count; i++) {
            uint8_t keyByte = flipSign(key[i]);
            f(&keyByte, child[i]);
        }
    }

    // expose key array + child array to allow efficient migration from Node4
    uint8_t *getKey() {
        return key;
    }

    Node **getChild() {
        return child;
    }
};
