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
#if __cplusplus >= 201703L
#include <variant>
#else
#include "mpark/variant.hpp"
#endif

#include "alc.h"
#include "alext.h"

#include "refcount.h"
#include "ringbuf.h"
#include "device.h"
#include "source.h"

#define F_PI (3.14159265358979323846f)

#if !(__cplusplus >= 201703L)
namespace std {
using mpark::variant;
using mpark::get;
using mpark::get_if;
using mpark::holds_alternative;
} // namespace std
#endif

namespace alure {

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
    SOFT_source_resampler,
    SOFT_source_spatialize,

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


class ListenerImpl {
    ContextImpl *const mContext;

public:
    ListenerImpl(ContextImpl *ctx) : mContext(ctx) { }

    void setGain(ALfloat gain);

    void set3DParameters(const Vector3 &position, const Vector3 &velocity, std::pair<Vector3,Vector3> orientation);

    void setPosition(ALfloat x, ALfloat y, ALfloat z);
    void setPosition(const ALfloat *pos);

    void setVelocity(ALfloat x, ALfloat y, ALfloat z);
    void setVelocity(const ALfloat *vel);

    void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2);
    void setOrientation(const ALfloat *at, const ALfloat *up);
    void setOrientation(const ALfloat *ori);

    void setMetersPerUnit(ALfloat m_u);
};


using DecoderOrExceptT = std::variant<SharedPtr<Decoder>,std::runtime_error>;
using BufferOrExceptT = std::variant<Buffer,std::runtime_error>;

class ContextImpl {
    static ContextImpl *sCurrentCtx;
    static thread_local ContextImpl *sThreadCurrentCtx;

public:
    static void MakeCurrent(ContextImpl *context);
    static ContextImpl *GetCurrent() { return sThreadCurrentCtx ? sThreadCurrentCtx : sCurrentCtx; }

    static void MakeThreadCurrent(ContextImpl *context);
    static ContextImpl *GetThreadCurrent() { return sThreadCurrentCtx; }

private:
    ListenerImpl mListener;
    ALCcontext *mContext;
    std::stack<ALuint> mSourceIds;

    struct PendingFuture { BufferImpl *mBuffer;  SharedFuture<Buffer> mFuture; };

    DeviceImpl *const mDevice;
    Vector<PendingFuture> mFutureBuffers;
    Vector<UniquePtr<BufferImpl>> mBuffers;
    Vector<UniquePtr<SourceGroupImpl>> mSourceGroups;
    std::deque<SourceImpl> mAllSources;
    Vector<SourceImpl*> mFreeSources;
    Vector<SourceBufferUpdateEntry> mPlaySources;
    Vector<SourceStreamUpdateEntry> mStreamSources;

    Vector<SourceImpl*> mStreamingSources;
    std::mutex mSourceStreamMutex;

    std::atomic<std::chrono::milliseconds> mWakeInterval;
    std::mutex mWakeMutex;
    std::condition_variable mWakeThread;

    SharedPtr<MessageHandler> mMessage;

    struct PendingBuffer {
        BufferImpl *mBuffer;
        SharedPtr<Decoder> mDecoder;
        ALenum mFormat;
        ALuint mFrames;
        Promise<Buffer> mPromise;

        ~PendingBuffer() { }
    };
    RingBuffer mPendingBuffers;

    std::atomic<bool> mQuitThread;
    std::thread mThread;
    void backgroundProc();

    RefCount mRefs;

    Vector<String> mResamplers;

    bool mHasExt[AL_EXTENSION_MAX];

    std::once_flag mSetExts;
    void setupExts();

    DecoderOrExceptT findDecoder(StringView name);
    BufferOrExceptT doCreateBuffer(StringView name, Vector<UniquePtr<BufferImpl>>::iterator iter, SharedPtr<Decoder> decoder);
    BufferOrExceptT doCreateBufferAsync(StringView name, Vector<UniquePtr<BufferImpl>>::iterator iter, SharedPtr<Decoder> decoder, Promise<Buffer> promise);

    bool mIsConnected : 1;
    bool mIsBatching : 1;

public:
    ContextImpl(ALCcontext *context, DeviceImpl *device);
    ~ContextImpl();

    ALCcontext *getContext() const { return mContext; }
    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }

    bool hasExtension(ALExtension ext) const { return mHasExt[ext]; }

    LPALGETSTRINGISOFT alGetStringiSOFT;
    LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;
    LPALGETSOURCEDVSOFT alGetSourcedvSOFT;

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

    void addPlayingSource(SourceImpl *source, ALuint id);
    void addPlayingSource(SourceImpl *source);
    void removePlayingSource(SourceImpl *source);

    void addStream(SourceImpl *source);
    void removeStream(SourceImpl *source);
    void removeStreamNoLock(SourceImpl *source);

    void freeSource(SourceImpl *source) { mFreeSources.push_back(source); }
    void freeSourceGroup(SourceGroupImpl *group);

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

    Device getDevice() { return Device(mDevice); }

    void destroy();

    void startBatch();
    void endBatch();

    Listener getListener() { return Listener(&mListener); }

    SharedPtr<MessageHandler> setMessageHandler(SharedPtr<MessageHandler> handler);
    SharedPtr<MessageHandler> getMessageHandler() const { return mMessage; }

    void setAsyncWakeInterval(std::chrono::milliseconds interval);
    std::chrono::milliseconds getAsyncWakeInterval() const { return mWakeInterval.load(); }

    SharedPtr<Decoder> createDecoder(StringView name);

    bool isSupported(ChannelConfig channels, SampleType type) const;

    ArrayView<String> getAvailableResamplers();
    ALsizei getDefaultResamplerIndex() const;

    Buffer getBuffer(StringView name);
    SharedFuture<Buffer> getBufferAsync(StringView name);
    void precacheBuffersAsync(ArrayView<StringView> names);
    Buffer createBufferFrom(StringView name, SharedPtr<Decoder> decoder);
    SharedFuture<Buffer> createBufferAsyncFrom(StringView name, SharedPtr<Decoder> decoder);
    void removeBuffer(StringView name);
    void removeBuffer(Buffer buffer) { removeBuffer(buffer.getName()); }

    Source createSource();

    AuxiliaryEffectSlot createAuxiliaryEffectSlot();

    Effect createEffect();

    SourceGroup createSourceGroup(StringView name);
    SourceGroup getSourceGroup(StringView name);

    void setDopplerFactor(ALfloat factor);

    void setSpeedOfSound(ALfloat speed);

    void setDistanceModel(DistanceModel model);

    void update();
};


inline void CheckContext(const ContextImpl *ctx)
{
    if(EXPECT(ctx != ContextImpl::GetCurrent(), false))
        throw std::runtime_error("Called context is not current");
}

} // namespace alure

#endif /* CONTEXT_H */
