#pragma once
/**
 * Mix of macro magic and template magic to generate a switch statement for a list of node types at compile time
 */
#include <boost/preprocessor/repetition.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/comparison/not_equal.hpp>
#include <boost/preprocessor/repetition/for.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/hana.hpp>
#include "../nodes/nodes.tcc"

// i <= n
#define PRED(r, state) \
   BOOST_PP_NOT_EQUAL( \
      BOOST_PP_TUPLE_ELEM(2, 0, state), \
      BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2, 1, state)) \
   ) \
   /**/
// i++
#define OP(r, state) \
   ( \
      BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(2, 0, state)), \
      BOOST_PP_TUPLE_ELEM(2, 1, state) \
   ) \
   /**/
// combine two MACROS
#define COMB_(x, y) x ## y
#define COMB(x, y)  COMB_(x,y)
// Macro for class T1, class T2, ...
#define MACRO_CLASS_TYPES(c)  COMB(class T,c)
#define MACRO_CLASS_TYPES_(r, state) MACRO_CLASS_TYPES(BOOST_PP_TUPLE_ELEM(2, 0, state)),
#define CLASS_TYPES(a, b) BOOST_PP_FOR((a, BOOST_PP_SUB(b,1)), PRED, OP, MACRO_CLASS_TYPES_) MACRO_CLASS_TYPES(b)
// Macro for T1, T2,...
#define MACRO_TYPES(c)  COMB(T,c)
#define MACRO_TYPES_(r, state) MACRO_TYPES(BOOST_PP_TUPLE_ELEM(2, 0, state)),
#define TYPES(a, b) BOOST_PP_FOR((a, BOOST_PP_SUB(b,1)), PRED, OP, MACRO_TYPES_) MACRO_TYPES(b)
// Macro for case T1: ...
#define MACRO_SWITCH_CASE(r, state) case MACRO_TYPES(BOOST_PP_TUPLE_ELEM(2, 0, state))::type::NodeType: \
        return compile_time_case_impl<NTL, MACRO_TYPES(BOOST_PP_TUPLE_ELEM(2, 0, state))::type::NodeType, F>(n, f);
#define SWITCH_CASES(a, b) BOOST_PP_FOR((a, b), PRED, OP, MACRO_SWITCH_CASE)

// Macro for one switch case for a number of nodes
#define SWITCH_IMPL(num) template<CLASS_TYPES(1,num)> \
struct compile_time_switch_s<boost::hana::tuple<TYPES(1,num)>> { \
    using NTL=boost::hana::tuple<TYPES(1,num)>; \
    template<typename F> \
    __attribute__((always_inline)) static void apply(Node *n, F f) { \
        switch (n->type) { \
            SWITCH_CASES(1,num) \
        } \
        __builtin_unreachable(); \
    } \
};
// Macro for generating switch statements for arbitrary many node types
#define MACRO_SWITCH_IMPL_(r, state) SWITCH_IMPL(BOOST_PP_TUPLE_ELEM(2, 0, state))
#define SWITCH_IMPLS(nums) BOOST_PP_FOR((1, nums), PRED, OP, MACRO_SWITCH_IMPL_)

namespace hana = boost::hana;
using namespace hana::literals;

/**
 * compile time switch for Node types
 */
template<class NTL, size_t Case, typename F>
__attribute__((always_inline)) inline void compile_time_case_impl(Node *n, F f) {
    constexpr auto types = NTL();
    constexpr auto res = hana::filter(types, [](auto a) {
        using T=typename decltype(a)::type;
        return std::integral_constant<bool, T::NodeType == Case>();
    });
    constexpr auto result = res[0_c];
    using NodeT=typename decltype(result)::type;
    f((NodeT *) n);
}

template<class NTL>
struct compile_time_switch_s {
};

SWITCH_IMPLS(50)

template<typename NTL, typename F>
__attribute__((always_inline)) inline void compile_time_switch(Node *n, F func) {
    compile_time_switch_s<typename std::remove_cv<NTL>::type>::apply(n, func);
}
