#pragma once

/**
 * Node256: standard implementation, stores up to 256 ptrs: For lookup: key byte -> array offset
 */
class Node256 : public Node {
    class Node256Iterator {
        Node256 *node;
        size_t pos;
    public:
        Node256Iterator() : Node256Iterator(nullptr, 0) {}

        Node256Iterator(Node256 *node, size_t pos) : node(node), pos(pos) {

        }

        Node256Iterator &operator++() {
            pos++;
            for (; pos < 256; pos++) {
                if (node->child[pos] != nullptr) {
                    return *this;
                }
            }
            return *this;
        }

        Node *operator*() const {
            return node->child[pos];
        }

        bool operator==(const Node256Iterator &other) {
            return other.node == this->node && other.pos == this->pos;
        }

        bool operator!=(const Node256Iterator &other) {
            return !(*this == other);
        }
    };

    Node *child[256];
public:
    static constexpr uint8_t NodeType = 3;
    using iterator_t=Node256Iterator;

    Node256() : Node(NodeType, 1) {
        memset(child, 0, sizeof(child));
    }

    explicit Node256(size_t /*levels*/) : Node256() {

    }

    iterator_t begin() {
        if (child[0] != nullptr)return Node256Iterator(this, 0);
        uint8_t zero = 0;
        return first_geq(&zero);
    }

    iterator_t end() {
        return Node256Iterator(this, 256);
    }

    iterator_t first_geq(uint8_t *key) {
        uint8_t keyByte = key[0];
        for (unsigned i = keyByte; i < 256; i++)
            if (child[i] != nullptr)
                return Node256Iterator(this, i);
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        return &(this->child[key[0]]);
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *keyByte) {
        return this->child[*keyByte];
    }

    insert_result_t insert(uint8_t *key, Node *newChild) {
        this->count++;
        this->child[key[0]] = newChild;
        return success;
    }

    template<typename F>
    void iterateOver(F f) {
        for (uint32_t i = 0; i < 256; i++) {
            uint8_t keyByte = i;
            if (child[i]) {
                f(&keyByte, child[i]);
            }
        }
    }
};
