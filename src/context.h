#ifndef CONTEXT_H
#define CONTEXT_H

#include "main.h"

#include <condition_variable>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <stack>
#include <queue>
#include <set>

#include "alc.h"
#include "alext.h"

#include "refcount.h"
#include "ringbuf.h"
#include "device.h"

namespace alure {

class ALDevice;
class ALBuffer;
class ALSource;
class ALSourceGroup;

enum ALExtension {
    EXT_EFX,

    EXT_FLOAT32,
    EXT_MCFORMATS,
    EXT_BFORMAT,

    EXT_MULAW,
    EXT_MULAW_MCFORMATS,
    EXT_MULAW_BFORMAT,

    SOFT_loop_points,
    SOFT_source_latency,

    AL_EXTENSION_MAX
};

// Batches OpenAL updates while the object is alive, if batching isn't already
// in progress.
class Batcher {
    ALCcontext *mContext;

public:
    Batcher(ALCcontext *context) : mContext(context) { }
    Batcher(Batcher&& rhs) : mContext(rhs.mContext) { rhs.mContext = nullptr; }
    ~Batcher()
    {
        if(mContext)
            alcProcessContext(mContext);
    }
};

class ALContext : public Context, public Listener {
    static ALContext *sCurrentCtx;
    static thread_local ALContext *sThreadCurrentCtx;

public:
    static void MakeCurrent(ALContext *context);
    static ALContext *GetCurrent() { return sThreadCurrentCtx ? sThreadCurrentCtx : sCurrentCtx; }

    static void MakeThreadCurrent(ALContext *context);
    static ALContext *GetThreadCurrent() { return sThreadCurrentCtx; }

private:
    ALCcontext *mContext;
    std::stack<ALuint> mSourceIds;

    ALDevice *const mDevice;
    std::queue<ALSource*> mFreeSources;
    Vector<ALSource*> mUsedSources;

    typedef std::map<String,ALBuffer*> BufferMap;
    BufferMap mBuffers;

    typedef Vector<ALSourceGroup*> SourceGroupList;
    SourceGroupList mSourceGroups;

    RefCount mRefs;

    SharedPtr<MessageHandler> mMessage;

    bool mHasExt[AL_EXTENSION_MAX];

    typedef struct PendingBuffer {
        String mName;
        ALBuffer *mBuffer;
        SharedPtr<Decoder> mDecoder;
        ALenum mFormat;
        ALuint mFrames;

        ~PendingBuffer() { }
    } PendingBuffer;
    ll_ringbuffer_t *mPendingBuffers;

    std::set<ALSource*> mStreamingSources;
    std::mutex mSourceStreamMutex;

    std::atomic<ALuint> mWakeInterval;
    std::mutex mWakeMutex;
    std::condition_variable mWakeThread;

    std::mutex mContextMutex;

    volatile bool mQuitThread;
    std::thread mThread;
    void backgroundProc();

    std::once_flag mSetExts;
    void setupExts();

    bool mIsBatching;

public:
    ALContext(ALCcontext *context, ALDevice *device);
    virtual ~ALContext();

    ALCcontext *getContext() const { return mContext; }
    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }

    bool hasExtension(ALExtension ext) const { return mHasExt[ext]; }

    LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;

    LPALGENEFFECTS alGenEffects;
    LPALDELETEEFFECTS alDeleteEffects;
    LPALISEFFECT alIsEffect;
    LPALEFFECTI alEffecti;
    LPALEFFECTIV alEffectiv;
    LPALEFFECTF alEffectf;
    LPALEFFECTFV alEffectfv;
    LPALGETEFFECTI alGetEffecti;
    LPALGETEFFECTIV alGetEffectiv;
    LPALGETEFFECTF alGetEffectf;
    LPALGETEFFECTFV alGetEffectfv;

    LPALGENFILTERS alGenFilters;
    LPALDELETEFILTERS alDeleteFilters;
    LPALISFILTER alIsFilter;
    LPALFILTERI alFilteri;
    LPALFILTERIV alFilteriv;
    LPALFILTERF alFilterf;
    LPALFILTERFV alFilterfv;
    LPALGETFILTERI alGetFilteri;
    LPALGETFILTERIV alGetFilteriv;
    LPALGETFILTERF alGetFilterf;
    LPALGETFILTERFV alGetFilterfv;

    LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
    LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
    LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
    LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
    LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv;
    LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
    LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv;
    LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti;
    LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv;
    LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf;
    LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv;

    ALuint getSourceId(ALuint maxprio);
    void insertSourceId(ALuint id) { mSourceIds.push(id); }

    void addStream(ALSource *source);
    void removeStream(ALSource *source);

    void freeSource(ALSource *source);
    void freeSourceGroup(ALSourceGroup *group);

    Batcher getBatcher()
    {
        if(mIsBatching)
            return Batcher(nullptr);
        alcSuspendContext(mContext);
        return Batcher(mContext);
    }

    virtual Device *getDevice() final;

    virtual void destroy() final;

    virtual void startBatch() final;
    virtual void endBatch() final;

    virtual Listener *getListener() final;

    virtual SharedPtr<MessageHandler> setMessageHandler(SharedPtr<MessageHandler> handler) final;
    virtual SharedPtr<MessageHandler> getMessageHandler() const final;

    virtual void setAsyncWakeInterval(ALuint msec) final;
    virtual ALuint getAsyncWakeInterval() const final;

    virtual SharedPtr<Decoder> createDecoder(const String &name) final;

    virtual Buffer *getBuffer(const String &name) final;
    virtual Buffer *getBufferAsync(const String &name) final;
    virtual void removeBuffer(const String &name) final;
    virtual void removeBuffer(Buffer *buffer) final;

    virtual Source *getSource() final;

    virtual AuxiliaryEffectSlot *createAuxiliaryEffectSlot() final;

    virtual Effect *createEffect() final;

    virtual SourceGroup *createSourceGroup() final;

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

    virtual void setMetersPerUnit(ALfloat m_u) final;
};


inline void CheckContext(ALContext *ctx)
{
    if(ctx != ALContext::GetCurrent())
        throw std::runtime_error("Called context is not current");
}

} // namespace alure

#endif /* CONTEXT_H */
