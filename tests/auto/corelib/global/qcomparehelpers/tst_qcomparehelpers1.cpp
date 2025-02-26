// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "tst_qcomparehelpers.h"

#define DECLARE_TYPE(Name, Type, RetType, Constexpr, Noex, Suffix, ...) \
class Templated ## Name \
{ \
public: \
    Constexpr Templated ## Name () {} \
\
private: \
    __VA_ARGS__ \
    friend Constexpr bool \
    comparesEqual(const Templated ## Name &lhs, X rhs) noexcept(Noex) \
    { Q_UNUSED(lhs); Q_UNUSED(rhs); return true; } \
    __VA_ARGS__ \
    friend Constexpr RetType \
    compareThreeWay(const Templated ## Name &lhs, X rhs) noexcept(Noex) \
    { Q_UNUSED(lhs); Q_UNUSED(rhs); return RetType::equivalent; } \
    Q_DECLARE_ ## Type ## _ORDERED ## Suffix (Templated ## Name, X, __VA_ARGS__) \
}; \
/* END */

#define DECLARE_TYPES_FOR_N_ATTRS(N, ...) \
DECLARE_TYPE(PartialConst ## N, PARTIALLY, Qt::partial_ordering, constexpr, \
             true, _LITERAL_TYPE, __VA_ARGS__) \
DECLARE_TYPE(Partial ## N, PARTIALLY, Qt::partial_ordering, , true, , __VA_ARGS__) \
DECLARE_TYPE(PartialNonNoex ## N, PARTIALLY, Qt::partial_ordering, , false, \
             _NON_NOEXCEPT, __VA_ARGS__) \
DECLARE_TYPE(WeakConst ## N, WEAKLY, Qt::weak_ordering, constexpr, true, \
             _LITERAL_TYPE, __VA_ARGS__) \
DECLARE_TYPE(Weak ## N, WEAKLY, Qt::weak_ordering, , true, , __VA_ARGS__) \
DECLARE_TYPE(WeakNonNoex ## N, WEAKLY, Qt::weak_ordering, , false, \
             _NON_NOEXCEPT, __VA_ARGS__) \
DECLARE_TYPE(StrongConst ## N, STRONGLY, Qt::strong_ordering, constexpr, true, \
             _LITERAL_TYPE, __VA_ARGS__) \
DECLARE_TYPE(Strong ## N, STRONGLY, Qt::strong_ordering, , true, , __VA_ARGS__) \
DECLARE_TYPE(StrongNonNoex ## N, STRONGLY, Qt::strong_ordering, , false, \
             _NON_NOEXCEPT, __VA_ARGS__) \
/* END */

template <typename T>
using if_int = std::enable_if_t<std::is_same_v<T, int>, bool>;

// The code below tries to craft some artificial template constraints that
// would fit into 1-7 macro arguments.
DECLARE_TYPES_FOR_N_ATTRS(1, template <typename X>)
DECLARE_TYPES_FOR_N_ATTRS(2, template <typename X, if_int<X> = true>)
DECLARE_TYPES_FOR_N_ATTRS(3, template <typename X, std::enable_if_t<std::is_integral_v<X>,
                                                                    bool> = true>)
DECLARE_TYPES_FOR_N_ATTRS(4, template <typename X, std::enable_if_t<std::is_same_v<X, int>,
                                                                    bool> = true>)
DECLARE_TYPES_FOR_N_ATTRS(5, template <typename X,
                                       std::enable_if_t<std::disjunction_v<
                                                                std::is_same<X, int>,
                                                                std::is_floating_point<X>>,
                                                        bool> = true>)
DECLARE_TYPES_FOR_N_ATTRS(6, template <typename X,
                                       std::enable_if_t<std::disjunction_v<
                                                                std::is_same<X, int>,
                                                                std::is_same<X, short>>,
                                                        bool> = true>)
DECLARE_TYPES_FOR_N_ATTRS(7, template <typename X,
                                       std::enable_if_t<std::disjunction_v<
                                                                std::is_same<X, int>,
                                                                std::is_same<X, short>,
                                                                std::is_floating_point<X>>,
                                                        bool> = true>)

#undef DECLARE_TYPES_FOR_N_ATTRS
#undef DECLARE_TYPE

void tst_QCompareHelpers::compareWithAttributes()
{
#define COMPARE(ClassName) \
    do { \
        ClassName c; \
        QCOMPARE_EQ(c, 0); \
        QCOMPARE_LE(c, 0); \
        QCOMPARE_GE(0, c); \
    } while (false)

#define COMPARE_SET(N) \
    COMPARE(TemplatedPartialConst ## N); \
    COMPARE(TemplatedPartial ## N); \
    COMPARE(TemplatedPartialNonNoex ## N); \
    COMPARE(TemplatedWeakConst ## N); \
    COMPARE(TemplatedWeak ## N); \
    COMPARE(TemplatedWeakNonNoex ## N); \
    COMPARE(TemplatedStrongConst ## N); \
    COMPARE(TemplatedStrong ## N); \
    COMPARE(TemplatedStrongNonNoex ## N); \
    /* END */

    COMPARE_SET(1)
    COMPARE_SET(2)
    COMPARE_SET(3)
    COMPARE_SET(4)
    COMPARE_SET(5)
    COMPARE_SET(6)
    COMPARE_SET(7)

#undef COMPARE_SET
#undef COMPARE
}

void tst_QCompareHelpers::totallyOrderedWrapperBasics()
{
    Qt::totally_ordered_wrapper<int*> pi; // partially-formed
    pi = nullptr;
    QCOMPARE_EQ(pi.get(), nullptr);

    // Test that we can create a wrapper for void*.
    [[maybe_unused]] constexpr Qt::totally_ordered_wrapper<void*> voidWrp{nullptr};

    // test that operator*() works
    int val = 10;
    Qt::totally_ordered_wrapper<int*> intWrp{&val};
    QCOMPARE_EQ(*intWrp, 10);
    *intWrp = 20;
    QCOMPARE_EQ(*intWrp, 20);
    QCOMPARE_EQ(val, 20);
}
