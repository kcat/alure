#ifndef SOURCE_H
#define SOURCE_H

#include "main.h"

#include <mutex>
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
    std::unique_ptr<ALBufferStream> mStream;

    mutable std::mutex mMutex;
    volatile bool mIsAsync;

    bool mLooping;
    bool mPaused;
    ALuint64SOFT mOffset;
    ALfloat mPitch;
    ALfloat mGain;
    ALfloat mMinGain, mMaxGain;
    ALfloat mRefDist, mMaxDist;
    Vector3 mPosition;
    Vector3 mVelocity;
    Vector3 mDirection;
    Vector3 mOrientation[2];
    ALfloat mConeInnerAngle, mConeOuterAngle;
    ALfloat mConeOuterGain, mConeOuterGainHF;
    ALfloat mRolloffFactor, mRoomRolloffFactor;
    ALfloat mDopplerFactor;
    ALfloat mAirAbsorptionFactor;
    bool mRelative;
    bool mDryGainHFAuto;
    bool mWetGainAuto;
    bool mWetGainHFAuto;

    ALuint mDirectFilter;
    SendPropMap mEffectSlots;

    ALuint mPriority;

    void resetProperties();
    void applyProperties(bool looping, ALuint offset) const;

    ALint refillBufferStream();

    void setFilterParams(ALuint &filterid, const FilterParams &params);

public:
    ALSource(ALContext *context);
    virtual ~ALSource();

    ALuint getId() const { return mId; }

    void updateNoCtxCheck();
    bool updateAsync();

    virtual void play(Buffer *buffer) final;
    virtual void play(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint queuesize) final;
    virtual void stop() final;
    virtual void pause() final;
    virtual void resume() final;

    virtual bool isPlaying() const final;
    virtual bool isPaused() const final;

    virtual void setPriority(ALuint priority) final;
    virtual ALuint getPriority() const final
    { return mPriority; }

    virtual void setOffset(uint64_t offset) final;
    virtual uint64_t getOffset(uint64_t *latency=0) const final;

    virtual void setLooping(bool looping) final;
    virtual bool getLooping() const final
    { return mLooping; }

    virtual void setPitch(ALfloat pitch) final;
    virtual ALfloat getPitch() const final
    { return mPitch; }

    virtual void setGain(ALfloat gain) final;
    virtual ALfloat getGain() const final { return mGain; }

    virtual void setGainRange(ALfloat mingain, ALfloat maxgain) final;
    virtual ALfloat getMinGain() const final { return mMinGain; }
    virtual ALfloat getMaxGain() const final { return mMaxGain; }

    virtual void setDistanceRange(ALfloat refdist, ALfloat maxdist) final;
    virtual ALfloat getReferenceDistance() const final { return mRefDist; }
    virtual ALfloat getMaxDistance() const final { return mMaxDist; }

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setPosition(const ALfloat *pos) final;
    virtual Vector3 getPosition() const final { return mPosition; }

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setVelocity(const ALfloat *vel) final;
    virtual Vector3 getVelocity() const final { return mVelocity; }

    virtual void setDirection(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setDirection(const ALfloat *dir) final;
    virtual Vector3 getDirection() const final { return mDirection; }

    virtual void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) final;
    virtual void setOrientation(const ALfloat *at, const ALfloat *up) final;
    virtual void setOrientation(const ALfloat *ori) final;

    virtual void setConeAngles(ALfloat inner, ALfloat outer) final;
    virtual ALfloat getInnerConeAngle() const final { return mConeInnerAngle; }
    virtual ALfloat getOuterConeAngle() const final { return mConeOuterAngle; }

    virtual void setOuterConeGains(ALfloat gain, ALfloat gainhf=1.0f) final;
    virtual ALfloat getOuterConeGain() const final { return mConeOuterGain; }
    virtual ALfloat getOuterConeGainHF() const final { return mConeOuterGainHF; }

    virtual void setRolloffFactors(ALfloat factor, ALfloat roomfactor=0.0f) final;
    virtual ALfloat getRolloffFactor() const final { return mRolloffFactor; }
    virtual ALfloat getRoomRolloffFactor() const final { return mRoomRolloffFactor; }

    virtual void setDopplerFactor(ALfloat factor) final;
    virtual ALfloat getDopplerFactor() const final
    { return mDopplerFactor; }

    virtual void setRelative(bool relative) final;
    virtual bool getRelative() const final
    { return mRelative; }

    virtual void setAirAbsorptionFactor(ALfloat factor) final;
    virtual ALfloat getAirAbsorptionFactor() const final { return mAirAbsorptionFactor; }

    virtual void setGainAuto(bool directhf, bool send, bool sendhf) final;
    virtual bool getDirectGainHFAuto() const final { return mDryGainHFAuto; }
    virtual bool getSendGainAuto() const final { return mWetGainAuto; }
    virtual bool getSendGainHFAuto() const final { return mWetGainHFAuto; }

    virtual void setDirectFilter(const FilterParams &filter) final;
    virtual void setSendFilter(ALuint send, const FilterParams &filter) final;
    virtual void setAuxiliarySend(AuxiliaryEffectSlot *slot, ALuint send) final;
    virtual void setAuxiliarySendFilter(AuxiliaryEffectSlot *slot, ALuint send, const FilterParams &filter) final;

    virtual void update() final;

    virtual void release() final;
};

} // namespace alure

#endif /* SOURCE_H */
