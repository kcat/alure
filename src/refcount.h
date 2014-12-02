#ifndef REFCOUNT_H
#define REFCOUNT_H

#if __cplusplus >= 201103L
#include <atomic>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace alure {

#if __cplusplus >= 201103L
typedef std::atomic<long> RefCount;
#elif defined(_WIN32)
template<typename T>
class atomic {
    T mValue;

public:
    atomic() { }
    atomic(T value) : mValue(value) { }

    T operator++() volatile { return InterlockedIncrement(&mValues); }
    T operator--() volatile { return InterlockedDecrement(&mValues); }

    void store(const T &value) volatile { mValue = value; }
    T load() const volatile { return mValue; }
};
typedef atomic<LONG> RefCount;
#endif

} // namespace alure

#endif /* REFCOUNT_H */
