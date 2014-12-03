#ifndef ALURE_MAIN_H
#define ALURE_MAIN_H

#include "alure2.h"

#if __cplusplus < 201103L
#define final
#endif

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
