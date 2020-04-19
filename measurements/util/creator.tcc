#pragma once
/*
For measurments, we want to create a node under test directly from  key value pairs. 
However, node creation varies, this is why the (specialized) creator struct is responsible.

create(std::vector<std::pair<uint64_t, Node *>> &values, art_impl<C> &a, size_t levels):

1. values: A list of key(uint64_t)/value(child ptr) pairs that should be stored inside the created node
2. a: The art instance, for which this node is created (required for rewired nodes)
3. levels: How many nodes should the created node span?

The responsible implementation will insert all key value pairs.
*/
template<class N>
struct creator {
    template<class C>
    static N *create(std::vector<std::pair<uint64_t, Node *>> &values, art_impl<C> &/*a*/, size_t levels) {
        N *underTest = NodeProperties<N>::creator::create(levels);
        for (auto x: values) {
            uint8_t arr[16];
            key_props<uint64_t>::keyToBytes(x.first, arr);
            insert_result_t insertResult = underTest->insert(&arr[7 - levels], x.second);
            if (insertResult == failed)throw;
            if (insertResult == selfgrow) {
                underTest = NodeProperties<N>::grow_props::selfgrow(underTest);
                underTest->insert(&arr[7 - levels], x.second);
            }
        }
        return underTest;
    }
};

template<>
struct creator<Rewired64K> {

    template<class C>
    static Rewired64K *
    create(std::vector<std::pair<uint64_t, Node *>> &values_, art_impl<C> &a, size_t /*levels*/) {
        using NT=decltype(C::nodeTypeList);
        std::sort(values_.begin(), values_.end());
        std::vector<std::pair<uint8_t[8], Node *>> entries;
        for (auto v:values_) {
            std::pair<uint8_t[8], Node *> p;
            key_props<uint64_t>::keyToBytes(v.first, p.first);
            p.second = v.second;
            entries.push_back(p);
        }
        Node *n = migrate_util<C>::createSubtree(entries, 5, 7, 0, entries.size(), a);
        Rewired64K *created;
        compile_time_switch<NT>(n, [&](auto x) {
            using localNodeType=typename std::remove_pointer<decltype(x)>::type;

            created = (Rewired64K *) migrate<localNodeType, Rewired64K, C>::apply(x, a, 5, 2);
        });
        return created;
    }
};

template<>
struct creator<Rewired16M> {
    template<class C>
    static Rewired16M *
    create(std::vector<std::pair<uint64_t, Node *>> &values_, art_impl<C> &a, size_t /*levels*/) {
        Rewired16M *to = Rewired16M::create(a.getRT());
        std::vector<std::pair<uint64_t, Node *>> values;
        std::sort(values_.begin(), values_.end());
        uint8_t currentFirstByte = 0;
        for (auto x:values_) {
            uint8_t arr[16];
            key_props<uint64_t>::keyToBytes(x.first, arr);
            if (arr[4] != currentFirstByte) {
                if (!values.empty()) {
                    Rewired64K *ml2 = creator<Rewired64K>::create(values, a, 2);
                    to->embed_existing_64K(currentFirstByte, *ml2);
                }
                values.clear();
                currentFirstByte = arr[4];
            }
            values.emplace_back(x.first, x.second);
        }
        if (!values.empty()) {
            Rewired64K *ml2 = creator<Rewired64K>::create(values, a, 2);
            to->embed_existing_64K(currentFirstByte, *ml2);
            delete ml2;
        }
        return to;
    }
};