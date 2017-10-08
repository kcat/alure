#ifndef ALURE_MAIN_H
#define ALURE_MAIN_H

#include "alure2.h"


#define DECL_THUNK0(ret, C, Name, cv) \
ret C::Name() cv { return pImpl->Name(); }
#define DECL_THUNK1(ret, C, Name, cv, T1)                                     \
ret C::Name(T1 a) cv                                                          \
{                                                                             \
    using _t1 = T1;                                                           \
    return pImpl->Name(std::forward<_t1&&>(a));                               \
}
#define DECL_THUNK2(ret, C, Name, cv, T1, T2)                                 \
ret C::Name(T1 a, T2 b) cv                                                    \
{                                                                             \
    using _t1 = T1; using _t2 = T2;                                           \
    return pImpl->Name(std::forward<_t1&&>(a), std::forward<_t2&&>(b));       \
}
#define DECL_THUNK3(ret, C, Name, cv, T1, T2, T3)                             \
ret C::Name(T1 a, T2 b, T3 c) cv                                              \
{                                                                             \
    using _t1 = T1; using _t2 = T2; using _t3 = T3;                           \
    return pImpl->Name(std::forward<_t1&&>(a), std::forward<_t2&&>(b),        \
                       std::forward<_t3&&>(c));                               \
}
#define DECL_THUNK6(ret, C, Name, cv, T1, T2, T3, T4, T5, T6)                 \
ret C::Name(T1 a, T2 b, T3 c, T4 d, T5 e, T6 f) cv                            \
{                                                                             \
    using _t1 = T1; using _t2 = T2; using _t3 = T3;                           \
    using _t4 = T4; using _t5 = T5; using _t6 = T6;                           \
    return pImpl->Name(std::forward<_t1&&>(a), std::forward<_t2&&>(b),        \
                       std::forward<_t3&&>(c), std::forward<_t4&&>(d),        \
                       std::forward<_t5&&>(e), std::forward<_t6&&>(f));       \
}

#ifdef __GNUC__
#define EXPECT(x, y) __builtin_expect((x), (y))
#else
#define EXPECT(x, y) (x)
#endif

#endif /* ALURE_MAIN_H */
