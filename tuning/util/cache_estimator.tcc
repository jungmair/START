#pragma once

//cache-relevant information about a node
struct node_cache_info {
    //how often is this node accessed, when all leaves are accessed once?
    size_t accesses;
    //cache state for the node header
    caching_state headerCacheState;
    //cache state for the node's body
    caching_state mainCacheState;
    //pointer to the node
    Node *node;
    //how big is the node
    int64_t size;
};
//compare two cache infos only by their access counts
inline bool operator>(const node_cache_info &lhs, const node_cache_info &rhs) {
    return lhs.accesses > rhs.accesses;
}

//class for estimating which node resides in which cache level (or none at all)
template<class Config>
class cache_estimator {
    using NT=decltype(Config::nodeTypeList);

    static constexpr int64_t header_size = 64;
    int64_t cache_size;
    std::priority_queue<node_cache_info, std::vector<node_cache_info>, std::greater<>> q;
    caching_state cache_type;
public:
    explicit cache_estimator(size_t cache_size, caching_state cache_type) : cache_size(cache_size),
                                                                            cache_type(cache_type) {}

    void registerNode(Node *n, size_t totalLeaves) {
        //enlist only the header...
        q.push({totalLeaves, cache_type, NO, n, header_size});
        cache_size -= header_size;
        //also enlist the nodes body
        compile_time_switch<NT>(n, [&](auto x) {
            int64_t remainingSize = std::max(0l, ((int64_t) sizeof(*x)) - (int64_t) header_size);
            cache_size -= remainingSize;
            q.push({totalLeaves / n->count, NO, cache_type, n, remainingSize});
        });
        //drop the least important nodes from queue until cache size is reached again
        while (!q.empty() && cache_size < 0) {
            cache_size += (int64_t) q.top().size;
            q.pop();
        }

    }

    template<class F>
    void visitEstimatedNodes(const F &f) {
        //iterate over the priority deque and call f for every entry.
        //however, iterating over a priority_queue is not straightforward :(
        std::priority_queue<node_cache_info, std::vector<node_cache_info>, std::greater<>> q2;
        while (!q.empty()) {
            node_cache_info cc = q.top();
            q2.push(cc);
            f(cc.headerCacheState, cc.mainCacheState, cc.node);
            q.pop();
        }
        q = q2;
    }
};