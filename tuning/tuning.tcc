#pragma once

#include<queue>
#include <chrono>
#include "util/dp_data.tcc"
#include "../config/node-props.tcc"
#include "util/cache_estimator.tcc"
#include "util/tag_manager.tcc"
//tuning happens here: cache estimations, dynamic programming and introducing multilevel nodes
template<class Config>
class tuning {
    using NT=decltype(Config::nodeTypeList);
    using K=typename Config::key_t;
    using KP=typename Config::key_props_t;
    art_impl<Config> &art;
    tag_manager tagManager;
    //cache estimators for L1/L2/L3
    cache_estimator<Config> l1CacheEstimator;
    cache_estimator<Config> l2CacheEstimator;
    cache_estimator<Config> l3CacheEstimator;


    template<class F_INFO, class F>
    void handlePrefix(Node *&n, dp_data &info, size_t depth, size_t handled, const F_INFO &fInfo, const F &f) {
        //handle node prefix during analysis
        size_t curr_prefixLen = n->prefixLength & ~0x8000u;//handle tagged nodes
        if (handled == curr_prefixLen) {
            //done with prefix -> go back to analyzeTree
            analyzeTree(n, info, depth, true, fInfo, f);
            return;
        }
        dp_data localInfo;
        //set dp data
        localInfo.top().childCount = 1;
        localInfo.next_free_level = info.next_free_level;
        //handlePrefix recursively byte per byte until done
        handlePrefix(n, localInfo, depth + 1, handled + 1, fInfo, f);
        localInfo.top().totalLeaves = localInfo.infos[1].totalLeaves;
        //accumulate dp data
        info.addShifted(localInfo, 1);
    }

    template<class localNodeType>
    void decide(dp_data &localInfo, size_t curr_depth, Node *n, replacement_decision &replacementDecision) {
        //set base cost parameters. Some of these values will be updated
        cost_parameter cost_parameters={
            .lookups = localInfo.top().totalLeaves,
            .headerCacheStatus = tagManager.getTag(n).headerCacheStatus,
            .mainCacheStatus=tagManager.getTag(n).mainCacheStatus,
            .keyLookups = (curr_depth != (KP::maxKeySize() - 1)) ? localInfo.top().leaves : 0
        };
        //calculate current costs based on the base cost parameters
        localInfo.top().cost = NodeProperties<localNodeType>().cost_model.getCost(cost_parameters) + localInfo.infos[1].cost;
        size_t totalLeavesInLevels = localInfo.top().leaves;
        size_t l1_cached = localInfo.top().cached_l1;
        size_t l2_cached = localInfo.top().cached_l2;
        size_t l3_cached = localInfo.top().cached_l3;
        //iterate over all levels
        for (size_t levels = 2; levels <= 8; levels++) {
            //check that we do not exceed the max key size
            if (curr_depth + levels > KP::maxKeySize()) {
                break;
            }
            //does a multilevel node break some other multilevel node?
            if (localInfo.canReplace(levels)){
                //accumulate cached data for levels
                l1_cached += localInfo.infos[levels - 1].cached_l1;
                l2_cached += localInfo.infos[levels - 1].cached_l2;
                l3_cached += localInfo.infos[levels - 1].cached_l3;
                //accumulate totalLeavesInLevels 
                totalLeavesInLevels += localInfo.infos[levels - 1].leaves;
                size_t totalChildren = localInfo.totalChildren(levels);
                //iterate over all node types
                boost::hana::for_each(Config::nodeTypeList, [&](const auto x) {
                    using replaceType=typename decltype(x)::type;
                    if (!NodeProperties<replaceType>().levelSizeProps.supports_levels(levels)) {
                        //current number of levels is not supported by this node type
                        return;
                    }
                    if (!NodeProperties<replaceType>().levelSizeProps.supports_children(levels, totalChildren)) {
                        //number of children is not supported
                        return;
                    }
                    bool mlNeedsKeyLookup = curr_depth + levels != KP::maxKeySize();
                    //how often will we have to load the full key?
                    cost_parameters.keyLookups = mlNeedsKeyLookup ? totalLeavesInLevels : 0;
                    //how big will the main part of the multilevel node be?
                    size_t mainSize = NodeProperties<replaceType>::size_estimater::getSize(totalChildren);
                    //check, if the space occupied by the total subtree in L1/L2/L3 before is sufficient
                    // to take the new node -> determine the resulting cache 
                    if (l1_cached >= mainSize) {
                        cost_parameters.mainCacheStatus = L1;
                    } else if (l2_cached >= mainSize) {
                        cost_parameters.mainCacheStatus = L2;
                    } else if (l3_cached >= mainSize) {
                        cost_parameters.mainCacheStatus = L3;
                    } else {
                        cost_parameters.mainCacheStatus = NO;
                    }
                    //calculate costs resulting when using a multilevel node
                    size_t alternative_cost =
                            NodeProperties<replaceType>().cost_model.getCost(cost_parameters) + localInfo.infos[levels].cost;
                    if (alternative_cost >= localInfo.top().cost) {
                        //alternative costs are higher than current ones (multilevel does not pay off)
                        return;
                    }
                    if (!filled_enough<replaceType>::apply(localInfo)) {
                        //some node types (e.g. rewired nodes require to be "filled enough")
                        return;
                    }
                    if (replacementDecision.bestTotalCost > alternative_cost) {
                        //by introducing the current multilevel node, we can improve overall costs
                        //-> update decision
                        replacementDecision.bestTotalCost = alternative_cost;
                        replacementDecision.bestNodeType = replaceType::NodeType;
                        replacementDecision.bestLevels = levels;
                    }
                });
            }
        }
    }

    template<class F, class F_INFO>
    void analyzeTree(Node *&n, dp_data &info, size_t depth, bool ignorePrefix, const F_INFO &fInfo, const F &f) {
        //generic analysis function that visits the tree bottom up
        dp_data localInfo;
        if (n == nullptr) {
            return;
        }
        if (isLeaf(n)) {
            info.top().leaves++;
            return;
        }
        if (n->prefixLength && !ignorePrefix) {
            handlePrefix(n, info, depth, 0, fInfo, f);
            return;
        }
        //derive localInfo from global info
        fInfo(info, localInfo, n, depth);

        localInfo.top().childCount = n->count;
        size_t toShift = 0;
        compile_time_switch<NT>(n,[&, depth](auto x) {
            size_t child_depth = depth + x->levels;
            toShift += x->levels;
            //iterate over all children and recursively analyze them
            x->iterateOver([&](uint8_t *, Node *&child) {
                analyzeTree(child, localInfo, child_depth, false, fInfo, f);
            });
            //update local dp data
            localInfo.top().totalLeaves =localInfo.top().leaves + localInfo.infos[1].totalLeaves;
        });
        //execute custom analysis/action on current node
        f(depth, n, localInfo);
        //aggregate dp info at next level
        info.addShifted(localInfo, toShift);
    }

    template<class F>
    void analyzeTree(const F &f) {
        dp_data info;
        analyzeTree(art.tree, info, 0, false, [](dp_data &/*a*/, dp_data &/*b*/, Node */*n*/, size_t /*d*/) {}, f);
    }

    template<class F_INFO, class F>
    void analyzeTree(const F_INFO &fInfo, const F &f) {
        dp_data info;
        analyzeTree(art.tree, info, 0, false, fInfo, f);
    }

public:
    explicit tuning(art_impl<Config> &art) : art(art), l1CacheEstimator(L1_CACHE_SIZE, L1),
                                                           l2CacheEstimator(L2_CACHE_SIZE, L2),
                                                           l3CacheEstimator(L3_CACHE_SIZE, L3) {}

    void tune() {
        using namespace std::chrono;
        size_t num_required_tags = 0;
        //visit whole tree and register all nodes to cache estimators
        analyzeTree([&](size_t /*curr_depth*/, Node *&n, dp_data &localInfo) {
            num_required_tags++;
            l1CacheEstimator.registerNode(n, localInfo.top().totalLeaves);
            l2CacheEstimator.registerNode(n, localInfo.top().totalLeaves);
            l3CacheEstimator.registerNode(n, localInfo.top().totalLeaves);
        });
        tagManager.resize(num_required_tags);
        //tag all nodes estimated to be in l1 cache
        l1CacheEstimator.visitEstimatedNodes([&](caching_state headState, caching_state mainState, Node *n) {
            dp_tag &tag = tagManager.addTag(n);
            tag.headerCacheStatus = std::min(tag.headerCacheStatus, headState);
            tag.mainCacheStatus = std::min(tag.mainCacheStatus, mainState);
        });
        //tag all nodes estimated to be in l2 cache
        l2CacheEstimator.visitEstimatedNodes([&](caching_state headState, caching_state mainState, Node *n) {
            dp_tag &tag = tagManager.addTag(n);
            tag.headerCacheStatus = std::min(tag.headerCacheStatus, headState);
            tag.mainCacheStatus = std::min(tag.mainCacheStatus, mainState);
        });
        //tag all nodes estimated to be in l3 cache
        l3CacheEstimator.visitEstimatedNodes([&](caching_state headState, caching_state mainState, Node *n) {
            dp_tag &tag = tagManager.addTag(n);
            tag.headerCacheStatus = std::min(tag.headerCacheStatus, headState);
            tag.mainCacheStatus = std::min(tag.mainCacheStatus, mainState);
        });
        //perform dynamic programming
        analyzeTree([&](size_t curr_depth, Node *&n, dp_data &localInfo) {
            compile_time_switch<NT>(n, [&](auto x) {
                using localNodeType=typename std::remove_pointer<decltype(x)>::type;
                //make sure the current node is tagged
                dp_tag &tag = tagManager.addTag(n);
                switch (tag.mainCacheStatus) {
                    case L1:
                        localInfo.top().cached_l1 += sizeof(localNodeType);
                        break;
                    case L2:
                        localInfo.top().cached_l2 += sizeof(localNodeType);
                        break;
                    case L3:
                        localInfo.top().cached_l3 += sizeof(localNodeType);
                        break;
                    default:
                        break;
                }
                //use the costmodel to make a (local) decision
                decide<localNodeType>(localInfo, curr_depth, x, tag.decision);
                if (tag.decision.valid()) {
                    //locally, the decision reduces cost -> update dp data
                    localInfo.top().cost = tag.decision.bestTotalCost;
                }

            });

        });
        //perform bottom up tree iteration, but top-most decisions are most relevant
        analyzeTree([&](dp_data &g, dp_data &l, Node *n, size_t curr_depth) {
            // propagate information which levels are allowed to migrate downwards
            if (g.next_free_level <= curr_depth) {
                const dp_tag &tag = tagManager.getTag(n);
                if (tag.decision.valid()) {
                    l.next_free_level = curr_depth + tag.decision.bestLevels;
                    l.doReplace = true;
                } else {
                    l.next_free_level = curr_depth + 1;
                }
            } else {
                l.next_free_level = g.next_free_level;
            }
        }, [&](size_t curr_depth, Node *&n, dp_data &localInfo) {
            compile_time_switch<NT>(n,
                                    [&](auto x) {
                                        using localNodeType=typename std::remove_pointer<decltype(x)>::type;
                                        const dp_tag &tag = tagManager.getTag(n);
                                        if (localInfo.doReplace && tag.decision.valid()) {
                                            //valid decision + decision is not overwritten by higher level
                                            boost::hana::for_each(Config::nodeTypeList, [&](const auto y) {
                                                //iterate over all node types to find the replacement type
                                                using replaceType=typename decltype(y)::type;
                                                if (replaceType::NodeType == tag.decision.bestNodeType) {
                                                        //execute decision: migrate
                                                        n = migrate<localNodeType, replaceType, Config>::apply(x,
                                                                                                               art,
                                                                                                               curr_depth,
                                                                                                               tag.decision.bestLevels);
                                                }

                                            });
                                        }
                                        localInfo.top().mL |= 1 << (n->levels - 1);
                                    });
            //remove tag from node
            tagManager.removeTag(n);
        });
    }

};
