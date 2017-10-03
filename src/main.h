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

namespace alure
{

template<typename T, typename T2>
inline T cast(T2 obj)
#ifndef ALURE_USE_RTTI
{ return obj ? static_cast<T>(obj) : 0; }
#else
{ return obj ? dynamic_cast<T>(obj) : 0; }
#endif

} // namespace alure

#endif /* ALURE_MAIN_H */
