#ifndef CONTEXT_H
#define CONTEXT_H

#include "main.h"

#include <condition_variable>
#include <unordered_map>
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
#include "source.h"

#define F_PI (3.14159265358979323846f)

namespace alure {

class ALDevice;
class ALBuffer;
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

    EXT_disconnect,

    EXT_SOURCE_RADIUS,
    EXT_STEREO_ANGLES,

    AL_EXTENSION_MAX
};

// Batches OpenAL updates while the object is alive, if batching isn't already
// in progress.
class Batcher {
    ALCcontext *mContext;

public:
    Batcher(ALCcontext *context) : mContext(context) { }
    Batcher(Batcher&& rhs) : mContext(rhs.mContext) { rhs.mContext = nullptr; }
    Batcher(const Batcher&) = delete;
    ~Batcher()
    {
        if(mContext)
            alcProcessContext(mContext);
    }

    Batcher& operator=(Batcher&&) = delete;
    Batcher& operator=(const Batcher&) = delete;
};

typedef std::unique_ptr<ll_ringbuffer_t,decltype(*ll_ringbuffer_free)> RingBufferPtr;

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
    std::deque<ALSource> mAllSources;
    std::queue<ALSource*> mFreeSources;
    Vector<ALSource*> mUsedSources;

    std::unordered_map<String,UniquePtr<ALBuffer>> mBuffers;

    Vector<UniquePtr<ALSourceGroup>> mSourceGroups;

    RefCount mRefs;

    SharedPtr<MessageHandler> mMessage;

    bool mHasExt[AL_EXTENSION_MAX];

    struct PendingBuffer {
        String mName;
        ALBuffer *mBuffer;
        SharedPtr<Decoder> mDecoder;
        ALenum mFormat;
        ALuint mFrames;

        ~PendingBuffer() { }
    };
    RingBufferPtr mPendingBuffers;

    Vector<ALSource*> mStreamingSources;
    std::mutex mSourceStreamMutex;

    std::atomic<ALuint> mWakeInterval;
    std::mutex mWakeMutex;
    std::condition_variable mWakeThread;

    std::mutex mContextMutex;

    std::atomic<bool> mQuitThread;
    std::thread mThread;
    void backgroundProc();

    std::once_flag mSetExts;
    void setupExts();

    bool mIsConnected;
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
    void removeStreamNoLock(ALSource *source);

    void freeSource(ALSource *source);
    void freeSourceGroup(ALSourceGroup *group);

    Batcher getBatcher()
    {
        if(mIsBatching)
            return Batcher(nullptr);
        alcSuspendContext(mContext);
        return Batcher(mContext);
    }

    std::unique_lock<std::mutex> getSourceStreamLock()
    { return std::unique_lock<std::mutex>(mSourceStreamMutex); }

    template<typename R, typename... Args>
    void send(R MessageHandler::* func, Args&&... args)
    { if(mMessage.get()) (mMessage.get()->*func)(std::forward<Args>(args)...); }

    Device *getDevice() override final;

    void destroy() override final;

    void startBatch() override final;
    void endBatch() override final;

    Listener *getListener() override final;

    SharedPtr<MessageHandler> setMessageHandler(SharedPtr<MessageHandler> handler) override final;
    SharedPtr<MessageHandler> getMessageHandler() const override final
    { return mMessage; }

    void setAsyncWakeInterval(ALuint msec) override final;
    ALuint getAsyncWakeInterval() const override final;

    SharedPtr<Decoder> createDecoder(const String &name) override final;

    Buffer *getBuffer(const String &name) override final;
    Buffer *getBufferAsync(const String &name) override final;
    void removeBuffer(const String &name) override final;
    void removeBuffer(Buffer *buffer) override final;

    Source *createSource() override final;

    AuxiliaryEffectSlot *createAuxiliaryEffectSlot() override final;

    Effect *createEffect() override final;

    SourceGroup *createSourceGroup() override final;

    void setDopplerFactor(ALfloat factor) override final;

    void setSpeedOfSound(ALfloat speed) override final;

    void setDistanceModel(DistanceModel model) override final;

    void update() override final;

    // Listener methods
    void setGain(ALfloat gain) final;

    void setPosition(ALfloat x, ALfloat y, ALfloat z) override final;
    void setPosition(const ALfloat *pos) override final;

    void setVelocity(ALfloat x, ALfloat y, ALfloat z) override final;
    void setVelocity(const ALfloat *vel) override final;

    void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) override final;
    void setOrientation(const ALfloat *at, const ALfloat *up) override final;
    void setOrientation(const ALfloat *ori) override final;

    void setMetersPerUnit(ALfloat m_u) override final;
};


inline void CheckContext(ALContext *ctx)
{
    if(ctx != ALContext::GetCurrent())
        throw std::runtime_error("Called context is not current");
}

} // namespace alure

#endif /* CONTEXT_H */
