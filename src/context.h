#ifndef CONTEXT_H
#define CONTEXT_H

#include "alure2.h"

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice;

class ALContext : public Context {
    static ALContext *sCurrentCtx;

    ALCcontext *mContext;

    ALDevice *mDevice;

    virtual ~ALContext();
public:
    ALContext(ALCcontext *context, ALDevice *device) : mContext(context), mDevice(device) { }

    ALCcontext *getContext() const { return mContext; }

    virtual Device *getDevice() final;

    virtual void destroy() final;

    static bool MakeCurrent(ALContext *context);
};

} // namespace alure

#endif /* CONTEXT_H */
