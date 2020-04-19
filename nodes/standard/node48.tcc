#pragma once

/**
 * Node48: standard implementation
 */
class Node48 : public Node {
    static constexpr uint8_t emptyMarker = 48;

    class Node48Iterator {
        Node48 *node;
        size_t pos;
    public:
        Node48Iterator() : Node48Iterator(nullptr, 0) {}

        Node48Iterator(Node48 *node, size_t pos) : node(node), pos(pos) {

        }

        Node48Iterator &operator++() {
            pos++;
            for (; pos < 256; pos++) {
                if (node->childIndex[pos] != emptyMarker) {
                    return *this;
                }
            }
            return *this;
        }

        Node *operator*() const {
            return node->child[node->childIndex[pos]];
        }

        bool operator==(const Node48Iterator &other) {
            return other.node == this->node && other.pos == this->pos;
        }

        bool operator!=(const Node48Iterator &other) {
            return !(*this == other);
        }
    };

    uint8_t childIndex[256];
    Node *child[48];
public:
    using iterator_t=Node48Iterator;
    static constexpr uint8_t NodeType = 2;

    Node48() : Node(NodeType, 1) {
        memset(childIndex, emptyMarker, sizeof(childIndex));
        memset(child, 0, sizeof(child));
    }

    explicit Node48(size_t /*levels*/) : Node48() {

    }

    iterator_t begin() {
        uint8_t zero = 0;
        return first_geq(&zero);
    }

    iterator_t end() {
        return Node48Iterator(this, 256);
    }

    iterator_t first_geq(uint8_t *key) {
        uint8_t keyByte = key[0];
        for (unsigned i = keyByte; i < 256; i++)
            if (this->childIndex[i] != emptyMarker)
                return Node48Iterator(this, i);
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        if (this->childIndex[key[0]] != emptyMarker)
            return &this->child[this->childIndex[key[0]]];
        else {
            return nullptr;
        }

    }

    __attribute__((always_inline)) Node *lookup(uint8_t *keyByte) {
        if (this->childIndex[*keyByte] != emptyMarker) {
            return this->child[this->childIndex[*keyByte]];
        } else
            return nullptr;
    }

    insert_result_t insert(uint8_t *key, Node *newChild) {
        if (this->count < 48) {
            unsigned pos = this->count;
            if (this->child[pos])
                for (pos = 0; this->child[pos] != nullptr; pos++);
            this->child[pos] = newChild;
            this->childIndex[key[0]] = pos;
            this->count++;
            return success;
        }
        return failed;
    }

    template<typename F>
    void iterateOver(F f) {
        for (uint32_t i = 0; i < 256; i++) {
            if (childIndex[i] != emptyMarker) {
                uint8_t keyByte = i;
                f(&keyByte, child[childIndex[i]]);
            }
        }
    }

    //expose arrays for efficient migrations from Node16 to Node48 using memcpy
    uint8_t *getChildIndex() {
        return childIndex;
    }

    Node **getChild() {
        return child;
    }
};
