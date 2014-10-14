#ifndef CONTEXT_H
#define CONTEXT_H

#include "alure2.h"

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice;

#define CHECK_ACTIVE_CONTEXT(ctx) do {                                        \
    if((ctx) != ALContext::GetCurrent())                                      \
        throw std::runtime_error("Called context is not current");            \
} while(0)

class ALContext : public Context {
    static ALContext *sCurrentCtx;
public:
    static bool MakeCurrent(ALContext *context);
    static ALContext *GetCurrent() { return sCurrentCtx; }

private:
    ALCcontext *mContext;

    ALDevice *mDevice;

    virtual ~ALContext();
public:
    ALContext(ALCcontext *context, ALDevice *device) : mContext(context), mDevice(device) { }

    ALCcontext *getContext() const { return mContext; }

    virtual Device *getDevice() final;

    virtual void destroy() final;
};

} // namespace alure

#endif /* CONTEXT_H */
