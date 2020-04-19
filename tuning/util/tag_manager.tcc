#pragma once

//simple struct for storing a replacement decision
struct replacement_decision {
    size_t bestTotalCost;
    size_t bestNodeType;
    size_t bestLevels;

    bool valid() const {
        //check if there is a valid decision
        return bestTotalCost != std::numeric_limits<size_t>::max();
    }
};

struct dp_tag {
    //we misuse the prefixlen and prefix data for storing a "pointer"  to the tag
    //->store the real data inside the tag
    uint64_t prefixInt;
    //store (estimated) caching state of header and node body
    caching_state headerCacheStatus;
    caching_state mainCacheStatus;
    //store replacement decision
    replacement_decision decision;

    explicit dp_tag(Node *n) : prefixInt{}, headerCacheStatus{NO}, mainCacheStatus{NO},
                                        decision{std::numeric_limits<size_t>::max(), n->type, n->levels} {

    };

    constexpr dp_tag() : prefixInt{}, headerCacheStatus{NO}, mainCacheStatus{NO},
                                  decision{std::numeric_limits<size_t>::max(), 0, 0} {
    };

};

class tag_manager {
    std::vector<dp_tag> dp_tags;
    static constexpr dp_tag dummyTag = {};
public:
    static constexpr bool isTagged(Node *n) {
        return n->prefixLength & 0x8000u;
    }

    dp_tag &addTag(Node *n) {
        if (isTagged(n)) {
            //node is already tagged
            return dp_tags[n->prefix_int];
        } else {
            //node is not tagged ->tag
            //signal tag by setting top most bit of prefix length
            n->prefixLength |= 0x8000u;
            //create tag for node
            dp_tags.emplace_back(n);
            size_t tag_id = dp_tags.size() - 1;
            //move prefix data into node
            dp_tags[tag_id].prefixInt = n->prefix_int;
            //store tag_id in node
            n->prefix_int = tag_id;
            //return reference to tag
            return dp_tags[n->prefix_int];
        }
    }

    void removeTag(Node *n) {
        //remove tag from node and restore prefix data
        if (isTagged(n)) {
            dp_tag &tag = dp_tags[n->prefix_int];
            n->prefix_int = tag.prefixInt;
            n->prefixLength &= ~0x8000u;
        }
    }

    const dp_tag &getTag(Node *n) {
        //get tag reference for given node
        if (isTagged(n)) {
            //if there is a tag -> return reference to it
            return dp_tags[n->prefix_int];
        } else {
            //no tag available -> return dummy 
            return dummyTag;
        }
    }

    void resize(size_t s) {
        //resize vector of dp_tags
        dp_tags.resize(s);
    }

    tag_manager() : dp_tags{} {}
};