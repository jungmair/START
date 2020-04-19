#pragma once
/**
 * Compile time configuration for ART/START
 */
#include <memory>
#include <cassert>
#include "../nodes/nodes.tcc"
#include "key_props.tcc"
#include "storage/vec_storage_provider.tcc"
#include <boost/hana.hpp>
#include "../statistics.tcc"
#include "RTInfo.tcc"

namespace hana = boost::hana;

template<typename K, typename NTL, template<typename> class STORAGE, class STATISTICS>
struct ARTConfiguration {

    //node configuration
    static constexpr auto nodeTypeList = NTL();
    static constexpr size_t mem_constr = sizeof(Node4);
    //Key configuration
    using key_t=K;
    using key_props_t=key_props<K>;
    //Storage configuration
    using storage_provider_t=STORAGE<K>;
    using statistics_t=STATISTICS;
    using RT_t=RTInfo<K,NTL,STORAGE,STATISTICS>;
};

