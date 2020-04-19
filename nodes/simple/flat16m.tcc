#pragma once

/**
 * Straightforward implementation of a 3-level node with up to 2^24 child pointers
 * Only implemented for measuring purposes, this node type should not be used for START
 */
class Flat16M : public Node {
public:
    static constexpr uint8_t NodeType = 8;
    static constexpr size_t num_ptrs = 1ull << 24;
    Node *child[num_ptrs];

    class Flat16MIterator {
        Flat16M *node;
        size_t pos;
    public:
        Flat16MIterator() : Flat16MIterator(nullptr, 0) {}

        Flat16MIterator(Flat16M *node, size_t pos) : node(node), pos(pos) {

        }

        Flat16MIterator &operator++() {
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

        bool operator==(const Flat16MIterator &other) {
            return other.node == this->node && other.pos == this->pos;
        }

        bool operator!=(const Flat16MIterator &other) {
            return !(*this == other);
        }
    };

    using iterator_t=Flat16MIterator;
public:
    Flat16M() : Node(NodeType, 3) {
        memset(child, 0, sizeof(child));
    }

    explicit Flat16M(size_t /*levels*/) : Flat16M() {

    }

    iterator_t begin() {
        if (child[0] != nullptr)return Flat16MIterator(this, 0);
        uint32_t zero = 0;
        return first_geq((uint8_t*)&zero);
    }

    iterator_t end() {
        return Flat16MIterator(this, num_ptrs);
    }

    iterator_t first_geq(uint8_t *key) {
        uint32_t keyByte = ((uint32_t) key[0] * 256 + key[1]) * 256 + key[2];
        for (unsigned i = keyByte; i < num_ptrs; i++)
            if (child[i] != nullptr)
                return Flat16MIterator(this, i);
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        return &(this->child[((uint32_t) key[0] * 256 + key[1]) * 256 + key[2]]);
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *key) {
        return this->child[((uint32_t) key[0] * 256 + key[1]) * 256 + key[2]];
    }

    insert_result_t insert(uint8_t *key, Node *newChild) {
        this->count++;
        this->child[((uint32_t) key[0] * 256 + key[1]) * 256 + key[2]] = newChild;
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
