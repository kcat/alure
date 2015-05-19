#ifndef CONTEXT_H
#define CONTEXT_H

#include "main.h"

#include <stdexcept>
#include <mutex>
#include <stack>
#include <queue>
#include <set>

#include "alc.h"
#include "alext.h"

#include "refcount.h"
#include "device.h"

namespace alure {

class ALDevice;
class ALBuffer;
class ALSource;

enum ALExtension {
    EXT_EFX,

    EXT_FLOAT32,
    EXT_MCFORMATS,
    EXT_BFORMAT,

    EXT_MULAW,
    EXT_MULAW_MCFORMATS,
    EXT_MULAW_BFORMAT,

    SOFT_source_latency,

    AL_EXTENSION_MAX
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
    std::set<ALSource*> mUsedSources;

    RefCount mRefs;

    bool mHasExt[AL_EXTENSION_MAX];

    std::once_flag mSetExts;
    void setupExts();

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

    void freeSource(ALSource *source);

    virtual Device *getDevice() final;

    virtual void destroy() final;

    virtual void startBatch() final;
    virtual void endBatch() final;

    virtual Listener *getListener() final;

    virtual SharedPtr<Decoder> createDecoder(const std::string &name) final;

    virtual Buffer *getBuffer(const std::string &name) final;
    virtual void removeBuffer(const std::string &name) final;
    virtual void removeBuffer(Buffer *buffer) final;

    virtual Source *getSource() final;

    virtual AuxiliaryEffectSlot *createAuxiliaryEffectSlot() final;

    virtual Effect *createEffect() final;

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
