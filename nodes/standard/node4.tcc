#pragma once

/**
 * Node4: almost standard implementation (adds 4 byte of padding to avoid misaligned pointers)
 */
class Node4 : public Node {
    uint8_t key[4];
    //difference to original ART implementation: add 4 bytes padding, otherwise: misaligned pointers!
    uint8_t pad[4];
    Node *child[4];
public:
    using iterator_t=Node **;
    static constexpr uint8_t NodeType = 0;

    Node4() : Node(NodeType, 1) {
        memset(child, 0, sizeof(child));
    }

    Node4(size_t /*levels*/) : Node4() {

    }

    iterator_t begin() {
        return &child[0];
    }

    iterator_t end() {
        return &child[count];
    }

    iterator_t first_geq(uint8_t *key) {
        for (unsigned i = 0; i < this->count; i++)
            if (this->key[i] >= key[0])
                return &this->child[i];
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        for (unsigned i = 0; i < this->count; i++)
            if (this->key[i] == key[0])
                return &this->child[i];
        return nullptr;
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *keyByteArr) {
        uint8_t keyByte = keyByteArr[0];
        if (this->key[0] == keyByte) {
            return this->child[0];
        }
        if (this->count == 1)
            return nullptr;
        if (this->key[1] == keyByte) {
            return this->child[1];
        }
        if (this->count == 2)
            return nullptr;
        if (this->key[2] == keyByte) {
            return this->child[2];
        }
        if (this->count == 3)
            return nullptr;
        if (this->key[3] == keyByte) {
            return this->child[3];
        }
        return nullptr;
    }

    insert_result_t insert(uint8_t *key, Node *child) {
        uint8_t keyByte = key[0];
        if (this->count < 4) {
            // Insert element
            unsigned pos;
            for (pos = 0; (pos < this->count) && (this->key[pos] < keyByte); pos++);
            memmove(this->key + pos + 1, this->key + pos, this->count - pos);
            memmove(this->child + pos + 1, this->child + pos, (this->count - pos) * sizeof(uintptr_t));
            this->key[pos] = keyByte;
            this->child[pos] = child;
            this->count++;
            return success;
        }
        return failed;
    }

    template<typename F>
    void iterateOver(F f) {
        for (uint32_t i = 0; i < this->count; i++) {
            f(&key[i], child[i]);
        }
    }

    // expose key array + child array to allow efficient migration to Node16
    uint8_t *getKey() {
        return key;
    }

    Node **getChild() {
        return child;
    }
};

