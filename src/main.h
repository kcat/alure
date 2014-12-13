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

class Vector3 {
    ALfloat mValue[3];

public:
    Vector3() { }
    Vector3(const Vector3 &rhs)
    {
        mValue[0] = rhs.mValue[0];
        mValue[1] = rhs.mValue[1];
        mValue[2] = rhs.mValue[2];
    }
    Vector3(ALfloat val)
    {
        mValue[0] = val;
        mValue[1] = val;
        mValue[2] = val;
    }
    Vector3(ALfloat x, ALfloat y, ALfloat z)
    {
        mValue[0] = x;
        mValue[1] = y;
        mValue[2] = z;
    }
    Vector3(const ALfloat *vec)
    {
        mValue[0] = vec[0];
        mValue[1] = vec[1];
        mValue[2] = vec[2];
    }

    const ALfloat *getPtr() const
    { return mValue; }

    ALfloat& operator[](size_t i)
    { return mValue[i]; }
    const ALfloat& operator[](size_t i) const
    { return mValue[i]; }

    Vector3 operator-(const Vector3 &rhs) const
    {
        return Vector3(mValue[0] - rhs.mValue[0],
                       mValue[1] - rhs.mValue[1],
                       mValue[2] - rhs.mValue[2]);
    }

    ALfloat getLengthSquared() const
    { return mValue[0]*mValue[0] + mValue[1]*mValue[1] + mValue[2]*mValue[2]; }

    ALfloat getDistanceSquared(const Vector3 &pos) const
    { return (pos - *this).getLengthSquared(); }
};
static_assert(sizeof(Vector3) == sizeof(ALfloat[3]), "Bad Vector3 size");

} // namespace alure

#endif /* ALURE_MAIN_H */
