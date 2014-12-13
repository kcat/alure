#ifndef ALURE_MAIN_H
#define ALURE_MAIN_H

#include "alure2.h"

#include <cmath>

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

class Vector3 {
    ALfloat mValue[3];

public:
    Vector3() { }
    Vector3(const Vector3 &rhs) : mValue{rhs.mValue[0], rhs.mValue[1], rhs.mValue[2]}
    { }
    Vector3(ALfloat val) : mValue{val, val, val}
    { }
    Vector3(ALfloat x, ALfloat y, ALfloat z) : mValue{x, y, z}
    { }
    Vector3(const ALfloat *vec) : mValue{vec[0], vec[1], vec[2]}
    { }

    const ALfloat *getPtr() const
    { return mValue; }

    ALfloat& operator[](size_t i)
    { return mValue[i]; }
    const ALfloat& operator[](size_t i) const
    { return mValue[i]; }

#define ALURE_DECL_OP(op)                           \
    Vector3 operator op(const Vector3 &rhs) const   \
    {                                               \
        return Vector3(mValue[0] op rhs.mValue[0],  \
                       mValue[1] op rhs.mValue[1],  \
                       mValue[2] op rhs.mValue[2]); \
    }
    ALURE_DECL_OP(+)
    ALURE_DECL_OP(-)
    ALURE_DECL_OP(*)
    ALURE_DECL_OP(/)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                    \
    Vector3& operator op(const Vector3 &rhs) \
    {                                        \
        mValue[0] op rhs.mValue[0];          \
        mValue[1] op rhs.mValue[1];          \
        mValue[2] op rhs.mValue[2];          \
        return *this;                        \
    }
    ALURE_DECL_OP(+=)
    ALURE_DECL_OP(-=)
    ALURE_DECL_OP(*=)
    ALURE_DECL_OP(/=)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                    \
    Vector3 operator op(ALfloat scale) const \
    {                                        \
        return Vector3(mValue[0] op scale,   \
                       mValue[1] op scale,   \
                       mValue[2] op scale);  \
    }
    ALURE_DECL_OP(*)
    ALURE_DECL_OP(/)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)               \
    Vector3& operator op(ALfloat scale) \
    {                                   \
        mValue[0] op scale;             \
        mValue[1] op scale;             \
        mValue[2] op scale;             \
        return *this;                   \
    }
    ALURE_DECL_OP(*=)
    ALURE_DECL_OP(/=)
#undef ALURE_DECL_OP

    ALfloat getLengthSquared() const
    { return mValue[0]*mValue[0] + mValue[1]*mValue[1] + mValue[2]*mValue[2]; }
    ALfloat getLength() const
    { return sqrtf(getLengthSquared()); }

    ALfloat getDistanceSquared(const Vector3 &pos) const
    { return (pos - *this).getLengthSquared(); }
    ALfloat getDistance(const Vector3 &pos) const
    { return (pos - *this).getLength(); }
};
static_assert(sizeof(Vector3) == sizeof(ALfloat[3]), "Bad Vector3 size");

} // namespace alure

#endif /* ALURE_MAIN_H */
