#ifndef CONTEXT_H
#define CONTEXT_H

#include "alure2.h"

#include "alc.h"

#if __cplusplus >= 201103L
#include <atomic>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice;

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

#define CHECK_ACTIVE_CONTEXT(ctx) do {                                        \
    if((ctx) != ALContext::GetCurrent())                                      \
        throw std::runtime_error("Called context is not current");            \
} while(0)

class ALContext : public Context {
    static ALContext *sCurrentCtx;
#if __cplusplus >= 201103L
    static thread_local ALContext *sThreadCurrentCtx;
#elif defined(_WIN32)
    static __declspec(thread) ALContext *sThreadCurrentCtx;
#else
    static __thread ALContext *sThreadCurrentCtx;
#endif
public:
    static void MakeCurrent(ALContext *context);
    static ALContext *GetCurrent() { return sCurrentCtx; }

    static void MakeThreadCurrent(ALContext *context);
    static ALContext *GetThreadCurrent() { return sThreadCurrentCtx; }

private:
    ALCcontext *mContext;

    ALDevice *mDevice;

    RefCount mRefs;

    virtual ~ALContext();
public:
    ALContext(ALCcontext *context, ALDevice *device)
      : mContext(context), mDevice(device), mRefs(0)
    { }

    ALCcontext *getContext() const { return mContext; }
    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }

    virtual Device *getDevice() final;

    virtual void destroy() final;
};

} // namespace alure

#endif /* CONTEXT_H */
