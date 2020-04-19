#pragma once

#include "../../util/helpers.tcc"
//multilevel variant of Node4
class MultiNode4 : public Node {
    using int_t=uint32_t;
    //store key parts + child pointers
    int_t key[4];
    Node *child[4];
public:
    //as all child pointers are stored continously: simple iterator
    using iterator_t=Node **;
    static constexpr uint8_t NodeType = 6;

    MultiNode4(uint8_t levels) : Node(NodeType, levels) {
        memset(child, 0, sizeof(child));
    }

    iterator_t begin() {
        return &child[0];
    }

    iterator_t end() {
        return &child[count];
    }

    iterator_t first_geq(uint8_t *keyBytes) {
        //load key bytes as integer and find the first entry, that is greater or equal
        int_t asInt = IntHelper<int_t>::load(keyBytes, levels);
        for (unsigned i = 0; i < this->count; i++)
            if (this->key[i] >= asInt)
                return &this->child[i];
        return end();
    }

    Node **findChildPtr(uint8_t *keyBytes) {
        //load key bytes as integer
        int_t cmp = IntHelper<int_t>::load(keyBytes, levels);
        //compare it with all stored
        for (unsigned i = 0; i < this->count; i++)
            if (this->key[i] == cmp)
                return &this->child[i];
        return nullptr;
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *keyBytes) {
        //load bytes as integer
        int_t asInt = IntHelper<int_t>::load(keyBytes, levels);
        //compare it with all stored keys
        if (this->key[0] == asInt) {
            return this->child[0];
        }
        if (this->count == 1)
            return nullptr;
        if (this->key[1] == asInt) {
            return this->child[1];
        }
        if (this->count == 2)
            return nullptr;
        if (this->key[2] == asInt) {
            return this->child[2];
        }
        if (this->count == 3)
            return nullptr;
        if (this->key[3] == asInt) {
            return this->child[3];
        }
        return nullptr;
    }

    insert_result_t insert(uint8_t *keyBytes, Node *child) {
        if (this->count < 4) {
            int_t asInt = IntHelper<int_t>::load(keyBytes, levels);
            //calculate pos
            unsigned pos;
            for (pos = 0; (pos < this->count) && (this->key[pos] < asInt); pos++);
            //move other entries
            memmove(this->key + pos + 1, this->key + pos, (this->count - pos) * sizeof(int_t));
            memmove(this->child + pos + 1, this->child + pos, (this->count - pos) * sizeof(uintptr_t));
            //insert
            this->key[pos] = asInt;
            this->child[pos] = child;
            this->count++;
            return success;
        }
        //when already full -> in
        return failed;
    }

    template<typename F>
    void iterateOver(F f) {
        //simple iteration, call f for every entry
        for (uint32_t i = 0; i < this->count; i++) {
            uint8_t arr[sizeof(int_t)];
            IntHelper<int_t>::unload(arr, key[i]);
            f(arr, child[i]);
        }
    }
};

