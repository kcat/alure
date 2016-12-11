#ifndef SOURCE_H
#define SOURCE_H

#include "main.h"

#include <unordered_map>
#include <atomic>
#include <mutex>

#include "al.h"
#include "alext.h"

namespace alure {

class ALContext;
class ALBuffer;
class ALBufferStream;
class ALAuxiliaryEffectSlot;
class ALSourceGroup;

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
typedef std::unordered_map<ALuint,SendProps> SendPropMap;

class ALSource : public Source {
    ALContext *const mContext;
    ALuint mId;

    ALBuffer *mBuffer;
    UniquePtr<ALBufferStream> mStream;

    ALSourceGroup *mGroup;

    mutable std::mutex mMutex;
    std::atomic<bool> mIsAsync;

    std::atomic<bool> mPaused;
    bool mLooping;
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
    ALfloat mRadius;
    ALfloat mStereoAngles[2];
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
    ~ALSource();

    ALuint getId() const { return mId; }

    void updateNoCtxCheck();
    bool updateAsync();

    void setGroup(ALSourceGroup *group);
    void unsetGroup();

    void groupUpdate();
    void groupPropUpdate(ALfloat gain, ALfloat pitch);

    void checkPaused();
    void unsetPaused() { mPaused = false; }
    void makeStopped();

    void play(Buffer *buffer) override final;
    void play(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint queuesize) override final;
    void stop() override final;
    void pause() override final;
    void resume() override final;

    bool isPlaying() const override final;
    bool isPaused() const override final;

    void setPriority(ALuint priority) override final;
    ALuint getPriority() const override final
    { return mPriority; }

    void setOffset(uint64_t offset) override final;
    uint64_t getOffset(uint64_t *latency=0) const override final;

    void setLooping(bool looping) override final;
    bool getLooping() const override final
    { return mLooping; }

    void setPitch(ALfloat pitch) override final;
    ALfloat getPitch() const override final
    { return mPitch; }

    void setGain(ALfloat gain) override final;
    ALfloat getGain() const override final { return mGain; }

    void setGainRange(ALfloat mingain, ALfloat maxgain) override final;
    std::pair<ALfloat,ALfloat> getGainRange() const override final
    { return {mMinGain, mMaxGain}; }

    void setDistanceRange(ALfloat refdist, ALfloat maxdist) override final;
    std::pair<ALfloat,ALfloat> getDistanceRange() const override final
    { return {mRefDist, mMaxDist}; }

    void setPosition(ALfloat x, ALfloat y, ALfloat z) override final;
    void setPosition(const ALfloat *pos) override final;
    Vector3 getPosition() const final override { return mPosition; }

    void setVelocity(ALfloat x, ALfloat y, ALfloat z) override final;
    void setVelocity(const ALfloat *vel) override final;
    Vector3 getVelocity() const override final { return mVelocity; }

    void setDirection(ALfloat x, ALfloat y, ALfloat z) override final;
    void setDirection(const ALfloat *dir) override final;
    Vector3 getDirection() const override final { return mDirection; }

    void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) override final;
    void setOrientation(const ALfloat *at, const ALfloat *up) override final;
    void setOrientation(const ALfloat *ori) override final;
    std::pair<Vector3,Vector3> getOrientation() const override final
    { return {mOrientation[0], mOrientation[1]}; }

    void setConeAngles(ALfloat inner, ALfloat outer) override final;
    std::pair<ALfloat,ALfloat> getConeAngles() const override final
    { return {mConeInnerAngle, mConeOuterAngle}; }

    void setOuterConeGains(ALfloat gain, ALfloat gainhf=1.0f) override final;
    std::pair<ALfloat,ALfloat> getOuterConeGains() const override final
    { return {mConeOuterGain, mConeOuterGainHF}; }

    void setRolloffFactors(ALfloat factor, ALfloat roomfactor=0.0f) override final;
    std::pair<ALfloat,ALfloat> getRolloffFactors() const override final
    { return {mRolloffFactor, mRoomRolloffFactor}; }

    void setDopplerFactor(ALfloat factor) override final;
    ALfloat getDopplerFactor() const override final
    { return mDopplerFactor; }

    void setRelative(bool relative) override final;
    bool getRelative() const override final
    { return mRelative; }

    void setRadius(ALfloat radius) override final;
    ALfloat getRadius() const override final { return mRadius; }

    void setStereoAngles(ALfloat leftAngle, ALfloat rightAngle) override final;
    std::pair<ALfloat,ALfloat> getStereoAngles() const override final
    { return std::make_pair(mStereoAngles[0], mStereoAngles[1]); }

    void setAirAbsorptionFactor(ALfloat factor) override final;
    ALfloat getAirAbsorptionFactor() const override final { return mAirAbsorptionFactor; }

    void setGainAuto(bool directhf, bool send, bool sendhf) override final;
    std::tuple<bool,bool,bool> getGainAuto() const override final
    { return std::make_tuple(mDryGainHFAuto, mWetGainAuto, mWetGainHFAuto); }

    void setDirectFilter(const FilterParams &filter) override final;
    void setSendFilter(ALuint send, const FilterParams &filter) override final;
    void setAuxiliarySend(AuxiliaryEffectSlot *slot, ALuint send) override final;
    void setAuxiliarySendFilter(AuxiliaryEffectSlot *slot, ALuint send, const FilterParams &filter) override final;

    void update() override final;

    void release() override final;
};

} // namespace alure

#endif /* SOURCE_H */
