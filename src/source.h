#ifndef SOURCE_H
#define SOURCE_H

#include "main.h"

#include <map>

#include "al.h"
#include "alext.h"

namespace alure {

class ALContext;
class ALBuffer;
class ALBufferStream;
class ALAuxiliaryEffectSlot;

struct SendProps {
    ALAuxiliaryEffectSlot *mSlot;
    ALuint mFilter;

    SendProps(ALAuxiliaryEffectSlot *slot) : mSlot(slot), mFilter(AL_FILTER_NULL)
    { }
    SendProps(ALuint filter) : mSlot(0), mFilter(filter)
    { }
    SendProps(ALAuxiliaryEffectSlot *slot, ALuint filter) : mSlot(slot), mFilter(filter)
    { }
};
typedef std::map<ALuint,SendProps> SendPropMap;

class ALSource : public Source {
    ALContext *const mContext;
    ALuint mId;

    ALBuffer *mBuffer;
    ALBufferStream *mStream;

    bool mLooping;
    bool mPaused;
    ALuint64SOFT mOffset;
    ALfloat mPitch;
    ALfloat mGain;
    ALfloat mMinGain, mMaxGain;
    ALfloat mRefDist, mMaxDist;
    ALfloat mPosition[3];
    ALfloat mVelocity[3];
    ALfloat mDirection[3];
    ALfloat mOrientation[2][3];
    ALfloat mConeInnerAngle, mConeOuterAngle;
    ALfloat mConeOuterGain;
    ALfloat mRolloffFactor;
    ALfloat mDopplerFactor;
    bool mRelative;

    ALuint mDirectFilter;
    SendPropMap mEffectSlots;

    void resetProperties();
    void applyProperties(bool looping, ALuint offset) const;

    void setFilterParams(ALuint &filterid, const FilterParams &params);

public:
    ALSource(ALContext *context)
      : mContext(context), mId(0), mBuffer(0), mStream(0), mDirectFilter(AL_FILTER_NULL)
    { resetProperties(); }

    void updateNoCtxCheck();

    virtual void play(Buffer *buffer) final;
    virtual void play(Decoder *decoder, ALuint updatelen, ALuint queuesize) final;
    virtual void stop() final;
    virtual void pause() final;
    virtual void resume() final;

    virtual bool isPlaying() const final;
    virtual bool isPaused() const final;

    virtual void setOffset(uint64_t offset) final;
    virtual uint64_t getOffset(uint64_t *latency=0) const final;

    virtual void setLooping(bool looping) final;
    virtual bool getLooping() const final;

    virtual void setPitch(ALfloat pitch) final;

    virtual void setGain(ALfloat gain) final;
    virtual void setGainRange(ALfloat mingain, ALfloat maxgain) final;

    virtual void setDistanceRange(ALfloat refdist, ALfloat maxdist) final;

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setPosition(const ALfloat *pos) final;

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setVelocity(const ALfloat *vel) final;

    virtual void setDirection(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setDirection(const ALfloat *dir) final;

    virtual void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) final;
    virtual void setOrientation(const ALfloat *at, const ALfloat *up) final;
    virtual void setOrientation(const ALfloat *ori) final;

    virtual void setConeAngles(ALfloat inner, ALfloat outer) final;
    virtual void setOuterConeGain(ALfloat gain) final;

    virtual void setRolloffFactor(ALfloat factor) final;

    virtual void setDopplerFactor(ALfloat factor) final;

    virtual void setRelative(bool relative) final;

    virtual void setDirectFilter(const FilterParams &filter) final;
    virtual void setSendFilter(ALuint send, const FilterParams &filter) final;
    virtual void setAuxiliarySend(AuxiliaryEffectSlot *slot, ALuint send) final;
    virtual void setAuxiliarySendFilter(AuxiliaryEffectSlot *slot, ALuint send, const FilterParams &filter) final;

    virtual void update() final;

    virtual void release() final;
};

} // namespace alure

#endif /* SOURCE_H */
