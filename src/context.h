#ifndef CONTEXT_H
#define CONTEXT_H

#include "alure2.h"

#include <stdexcept>
#include <stack>
#include <queue>
#include <set>

#include "alc.h"
#include "alext.h"

#include "refcount.h"
#include "device.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice;
class ALBuffer;
class ALSource;

enum ALExtension {
    SOFT_source_latency,

    AL_EXTENTION_MAX
};

class ALContext : public Context, public Listener {
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
    static ALContext *GetCurrent() { return sThreadCurrentCtx ? sThreadCurrentCtx : sCurrentCtx; }

    static void MakeThreadCurrent(ALContext *context);
    static ALContext *GetThreadCurrent() { return sThreadCurrentCtx; }

private:
    ALCcontext *mContext;
    std::stack<ALuint> mSourceIds;

    ALDevice *mDevice;
    std::queue<ALSource*> mFreeSources;
    std::set<ALSource*> mUsedSources;

    RefCount mRefs;

    bool mFirstSet;
    bool mHasExt[AL_EXTENTION_MAX];

    void setupExts();

public:
    ALContext(ALCcontext *context, ALDevice *device);
    virtual ~ALContext();

    ALCcontext *getContext() const { return mContext; }
    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }

    void setCurrent();
    bool hasExtension(ALExtension ext) const { return mHasExt[ext]; }

    LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;

    ALuint getSourceId();
    void insertSourceId(ALuint id) { mSourceIds.push(id); }

    void freeSource(ALSource *source);

    virtual Device *getDevice() final;

    virtual void destroy() final;

    virtual void startBatch() final;
    virtual void endBatch() final;

    virtual Listener *getListener() final;

    virtual Decoder *createDecoder(const std::string &name) final;

    virtual Buffer *getBuffer(const std::string &name) final;
    virtual void removeBuffer(const std::string &name) final;
    virtual void removeBuffer(Buffer *buffer) final;

    virtual Source *getSource() final;
    virtual void finalize(Source *source) final;

    virtual void setDopplerFactor(ALfloat factor) final;

    virtual void setSpeedOfSound(ALfloat speed) final;

    virtual void setDistanceModel(DistanceModel model) final;

    virtual void update() final;

    // Listener methods
    virtual void setGain(ALfloat gain) final;

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setPosition(const ALfloat *pos) final;

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setVelocity(const ALfloat *vel) final;

    virtual void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) final;
    virtual void setOrientation(const ALfloat *at, const ALfloat *up) final;
    virtual void setOrientation(const ALfloat *ori) final;
};


inline void CheckContext(ALContext *ctx)
{
    if(ctx != ALContext::GetCurrent())
        throw std::runtime_error("Called context is not current");
}

inline void CheckContextDevice(ALDevice *device)
{
    ALContext *ctx = ALContext::GetCurrent();
    if(!ctx || device != ctx->getDevice())
        throw std::runtime_error("Called device is not current");
}

} // namespace alure

#endif /* CONTEXT_H */
