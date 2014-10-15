#ifndef CONTEXT_H
#define CONTEXT_H

#include "alure2.h"

#include <stdexcept>
#include <stack>
#include <queue>

#include "alc.h"

#include "refcount.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice;
class ALBuffer;
class ALSource;

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
    std::stack<ALuint> mSourceIds;

    ALDevice *mDevice;
    std::queue<ALSource*> mSources;

    RefCount mRefs;

    virtual ~ALContext();
public:
    ALContext(ALCcontext *context, ALDevice *device)
      : mContext(context), mDevice(device), mRefs(0)
    { }

    ALCcontext *getContext() const { return mContext; }
    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }

    ALuint getSourceId();
    void insertSourceId(ALuint id) { mSourceIds.push(id); }
    void insertSource(ALSource *source) { mSources.push(source); }

    virtual Device *getDevice() final;

    virtual void destroy() final;

    virtual Buffer *getBuffer(const std::string &name) final;
    virtual void removeBuffer(const std::string &name) final;
    virtual void removeBuffer(Buffer *buffer) final;

    virtual Source *getSource() final;
    virtual void finalize(Source *source) final;
};


inline void CheckContext(ALContext *ctx)
{
    ALContext *thrdctx = ALContext::GetThreadCurrent();
    if((thrdctx && ctx != thrdctx) || (!thrdctx && ctx != ALContext::GetCurrent()))
        throw std::runtime_error("Called context is not current");
}

void CheckContextDevice(ALDevice *device);

} // namespace alure

#endif /* CONTEXT_H */
