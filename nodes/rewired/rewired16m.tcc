#pragma once

#include <functional>
#include "rewired64k.tcc"
#include "../../util/rewiring/rewiring_provider_creator.h"

//rewired node storing up to 2^24 child pointers
class Rewired16M : public Node {
public:
    //iterator for Rewired16M nodes: iterates over embedded Rewired64K nodes and delegates element-wise iteration
    class Rewired16MIterator {
        //current node
        Rewired16M *node;
        //current position <=256
        size_t pos;
        //current embedded node
        Rewired64K *embedded;
        //iterator for current embedded node
        Rewired64K::iterator_t iterator64k;
    public:
        Rewired16MIterator() : Rewired16MIterator(nullptr, 0, Rewired64K::Rewired64KIterator(), nullptr) {}

        Rewired16MIterator(Rewired16M *node, size_t pos, Rewired64K::iterator_t iterator64k, Rewired64K *embedded)
                : node(node), pos(pos), embedded(embedded), iterator64k(iterator64k) {
        }

        Rewired16MIterator &operator++() {
            //increment iterator of embedded64k
            ++iterator64k;
            if (iterator64k == embedded->end()) {
                //finished with embedded 64k node -> increment position
                pos++;
                //iterate over embedded 64k nodes to find the next active
                for (; pos < 256; pos++) {
                    if (node->used[pos]) {
                        //active embedded 64k node
                        Rewired64K &rewired2 = node->embedded_64k[pos];
                        //set iterator to iterator of embedded 64k
                        iterator64k = rewired2.begin();
                        embedded = &rewired2;
                        return *this;
                    }
                }
                //no active 64k node anymore -> set to end
                *this = node->end();
            }
            return *this;
        }

        Node *operator*() const {
            //get child pointer at current location (delegate)
            return *iterator64k;
        }

        bool operator==(const Rewired16MIterator &other) {
            //compare iterators by node as well as position
            return other.node == this->node && other.pos == this->pos;
        }

        bool operator!=(const Rewired16MIterator &other) {
            return !(*this == other);
        }
    };
    using iterator_t=Rewired16MIterator;
    static constexpr uint8_t NodeType = 5;
    bool used[256];

private:
    uint64_t *array;
    //array for storing 
    Rewired64K embedded_64k[256];
    //reservation of virtual memory
    reservation res;
    //information necessary for creating new 2level nodes
    std::shared_ptr<rewiring_provider_creator> rewired_creator;

    __attribute__((always_inline)) static constexpr bool tagIsEqual(uint64_t a,uint64_t b){
        return (a & 0xffff) == (b & 0xffff);
    }
public:
    explicit Rewired16M(std::shared_ptr<rewiring_provider_creator> ptr) : Node(NodeType,3),
        used(),res(256 * 128*4096), rewired_creator(ptr) {
        //set array to reserved area of virtual address space
        array = (uint64_t *) res.getStart();
    }

    template<typename C>
    static Rewired16M *create(C &config) {
        return new Rewired16M(config.rewiring_creator);
    }

    Rewired64K::embedding_info getEmbeddingInfo(size_t prefix){
        //returns an embedding_info for creating an embedded rewired64k 
        //create rewiring provider
        std::shared_ptr<rewiring_provider> provider=rewired_creator->createProvider(128);
        //set virtual address of rewiring
        provider->setStart(reinterpret_cast<Page *>(array)+prefix*128);
        Rewired64K::embedding_info eInfo;
        //set location for Rewiring64K node
        eInfo.embeddedLocation=&embedded_64k[prefix];
        //set rewiring provider
        eInfo.rewiringProvider=provider;
        return eInfo;
    }

    void embed_existing_64K(uint8_t prefix, Rewired64K &existing) {
        //embedding a previous standalone Rewired64K node
        //move physical pages to new virtual range
        existing.move(((Page *) array) + 128 * prefix);
        //move node data
        embedded_64k[prefix]=std::move(existing);
        //update management data
        count += existing.count;
        used[prefix] = true;
    }

    size_t getSize() {
        size_t size = sizeof(Rewired16M);
        //add sizes of embedded rewired64k
        for (int prefix = 0; prefix <= 255; prefix++) {
            if (used[prefix]) {
                size += embedded_64k->getSize();
            }
        }
        return size;
    }

    iterator_t begin() {
        uint8_t zero[3] = {0, 0, 0};
        //find the first populated entry
        return first_geq(zero);
    }

    iterator_t end() {
        //return a iterator state representing the end
        return Rewired16MIterator(this, 256, Rewired64K::Rewired64KIterator(nullptr, 1 << 16), nullptr);
    }

    iterator_t first_geq(uint8_t *key) {
        for (int i = key[0]; i < 256; i++) {
            if (used[i]) {
                Rewired64K &rewired2 = embedded_64k[i];
                Rewired64K::iterator_t it = rewired2.first_geq(&key[1]);
                if (it == rewired2.end()) {
                    continue;
                }
                return Rewired16MIterator(this, i, it, &rewired2);
            }
        }
        //no entry located after the key was found
        return end();
    }

    Node **findChildPtr(uint8_t *key) {
        if (used[key[0]]) {
            //delegate to embedded 64k node
            return embedded_64k[key[0]].findChildPtr(key + 1);
        } else {
            //embedded 64k node not in place -> nullptr
            return nullptr;
        }
    }

    __attribute__((always_inline)) Node *lookup(uint8_t *keyByte) {
        //calculate offset from key bytes
        size_t original_offset=(((uint32_t) keyByte[0]) * 256 + (uint32_t) keyByte[1]) * 256 + (uint32_t) keyByte[2];
        size_t offset = original_offset;
        if (tagIsEqual(array[offset],original_offset)) {
            //tag matched -> return child pointer (untag by shifting)
            return (Node *) (array[offset] >> 16);
        }
        //no entry found at expected offset, try "off-by-one"
        offset++;
        if (offset % 512 == 0)return NULL;
        if (tagIsEqual(array[offset],original_offset)) {
            //tag matched -> return child pointer (untag by shifting)
            return (Node *) (array[offset] >> 16);
        }
        return NULL;
    }

    __attribute__((always_inline))  insert_result_t insert(uint8_t *key, Node *child) {
        //increment count
        count++;
        if (used[key[0]]) {
            //case 1: responsible embedded 64k rewired node already in place -> delegate
            return embedded_64k[key[0]].insert(key + 1, child);
        } else {
            //case 2: responsible embedded 64k rewired node not in place -> create
            used[key[0]] = true;
            Rewired64K* newEmbedded=Rewired64K::createEmbedded(getEmbeddingInfo(key[0]));
            //initialize
            page_squeezer squeezer;
            newEmbedded->initialize(squeezer);
            //insert current entry
            newEmbedded->insert(key + 1, child);
            return success;
        }
    }

    template<typename F>
    void iterateOver(F f) {
        //simple iteration over all entries
        for (int prefix = 0; prefix <= 255; prefix++) {
            if (used[prefix]) {
                //prefix is used -> iterate over embedded rewired64k nodes
                embedded_64k[prefix].iterateOver([&f, prefix](uint8_t *lowerKeyBytes, Node *&ptr) {
                    //compose key bytes and call f
                    uint8_t keyBytes[] = {static_cast<uint8_t>(prefix & 0xff), lowerKeyBytes[0], lowerKeyBytes[1]};
                    f(keyBytes, ptr);
                });
            }
        }
    }

    //allow access to clear_cache struct for measurement
    template<class T>
    friend
    struct clear_cache;
};
