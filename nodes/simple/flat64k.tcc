#pragma once

/**
 * Straightforward implementation of a 2-level node with up to 2^16 child pointers
 * Only implemented for measuring purposes, this node type should not be used for START
 */
class Flat64K : public Node {
public:
    static constexpr uint8_t NodeType = 7;
    static constexpr size_t num_ptrs = 1ull << 16;
    Node *child[num_ptrs];

    class Flat64KIterator {
        Flat64K *node;
        size_t pos;
    public:
        Flat64KIterator() : Flat64KIterator(nullptr, 0) {}

        Flat64KIterator(Flat64K *node, size_t pos) : node(node), pos(pos) {

        }

        Flat64KIterator &operator++() {
            pos++;
            for (; pos < num_ptrs; pos++) {
                if (node->child[pos] != nullptr) {
                    return *this;
                }
            }
            return *this;
        }

        Node *operator*() const {
            return node->child[pos];
        }

        bool operator==(const Flat64KIterator &other) {
            return other.node == this->node && other.pos == this->pos;
        }

        bool operator!=(const Flat64KIterator &other) {
            return !(*this == other);
        }
    };

    using iterator_t=Flat64KIterator;
public:
    Flat64K() : Node(NodeType, 2) {
        memset(child, 0, sizeof(child));
    }

    explicit Flat64K(size_t /*levels*/) : Flat64K() {

    }

    iterator_t begin() {
        if (child[0] != nullptr)return Flat64KIterator(this, 0);
        uint16_t zero = 0;
        return first_geq((uint8_t*)&zero);
    }

    iterator_t end() {
        return Flat64KIterator(this, num_ptrs);
    }

    iterator_t first_geq(uint8_t *key) {
        uint32_t keyByte = (uint32_t) key[0] * 256 + key[1];
        for (unsigned i = keyByte; i < num_ptrs; i++)
            if (child[i] != nullptr)
                return Flat64KIterator(this, i);
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        return &(this->child[(uint32_t) key[0] * 256 + key[1]]);
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *key) {
        return this->child[(uint32_t) key[0] * 256 + key[1]];
    }

    insert_result_t insert(uint8_t *key, Node *newChild) {
        this->count++;
        this->child[(uint32_t) key[0] * 256 + key[1]] = newChild;
        return success;
    }

    template<typename F>
    void iterateOver(F f) {
        for (uint32_t i = 0; i < num_ptrs; i++) {
            uint8_t arr[2];
            IntHelper<uint16_t>::unload(arr, i);
            f(arr, child[i]);
        }
    }
};

