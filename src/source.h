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

class ALSource {
    ALContext *const mContext;
    ALuint mId;

    ALBuffer *mBuffer;
    UniquePtr<ALBufferStream> mStream;

    ALSourceGroup *mGroup;

    mutable std::mutex mMutex;
    std::atomic<bool> mIsAsync;

    std::atomic<bool> mPaused;
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
    Spatialize mSpatialize;
    ALsizei mResampler;
    bool mLooping : 1;
    bool mRelative : 1;
    bool mDryGainHFAuto : 1;
    bool mWetGainAuto : 1;
    bool mWetGainHFAuto : 1;

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

    void play(Buffer buffer);
    void play(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint queuesize);
    void stop();
    void pause();
    void resume();

    bool isPlaying() const;
    bool isPaused() const;

    void setPriority(ALuint priority);
    ALuint getPriority() const { return mPriority; }

    void setOffset(uint64_t offset);
    std::pair<uint64_t,std::chrono::nanoseconds> getSampleOffsetLatency() const;
    std::pair<Seconds,Seconds> getSecOffsetLatency() const;

    void setLooping(bool looping);
    bool getLooping() const { return mLooping; }

    void setPitch(ALfloat pitch);
    ALfloat getPitch() const { return mPitch; }

    void setGain(ALfloat gain);
    ALfloat getGain() const { return mGain; }

    void setGainRange(ALfloat mingain, ALfloat maxgain);
    std::pair<ALfloat,ALfloat> getGainRange() const
    { return {mMinGain, mMaxGain}; }

    void setDistanceRange(ALfloat refdist, ALfloat maxdist);
    std::pair<ALfloat,ALfloat> getDistanceRange() const
    { return {mRefDist, mMaxDist}; }

    void set3DParameters(const Vector3 &position, const Vector3 &velocity, const Vector3 &direction);
    void set3DParameters(const Vector3 &position, const Vector3 &velocity, std::pair<Vector3,Vector3> orientation);

    void setPosition(ALfloat x, ALfloat y, ALfloat z);
    void setPosition(const ALfloat *pos);
    Vector3 getPosition() const { return mPosition; }

    void setVelocity(ALfloat x, ALfloat y, ALfloat z);
    void setVelocity(const ALfloat *vel);
    Vector3 getVelocity() const { return mVelocity; }

    void setDirection(ALfloat x, ALfloat y, ALfloat z);
    void setDirection(const ALfloat *dir);
    Vector3 getDirection() const { return mDirection; }

    void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2);
    void setOrientation(const ALfloat *at, const ALfloat *up);
    void setOrientation(const ALfloat *ori);
    std::pair<Vector3,Vector3> getOrientation() const
    { return {mOrientation[0], mOrientation[1]}; }

    void setConeAngles(ALfloat inner, ALfloat outer);
    std::pair<ALfloat,ALfloat> getConeAngles() const
    { return {mConeInnerAngle, mConeOuterAngle}; }

    void setOuterConeGains(ALfloat gain, ALfloat gainhf=1.0f);
    std::pair<ALfloat,ALfloat> getOuterConeGains() const
    { return {mConeOuterGain, mConeOuterGainHF}; }

    void setRolloffFactors(ALfloat factor, ALfloat roomfactor=0.0f);
    std::pair<ALfloat,ALfloat> getRolloffFactors() const
    { return {mRolloffFactor, mRoomRolloffFactor}; }

    void setDopplerFactor(ALfloat factor);
    ALfloat getDopplerFactor() const { return mDopplerFactor; }

    void setRelative(bool relative);
    bool getRelative() const { return mRelative; }

    void setRadius(ALfloat radius);
    ALfloat getRadius() const { return mRadius; }

    void setStereoAngles(ALfloat leftAngle, ALfloat rightAngle);
    std::pair<ALfloat,ALfloat> getStereoAngles() const
    { return std::make_pair(mStereoAngles[0], mStereoAngles[1]); }

    void set3DSpatialize(Spatialize spatialize);
    Spatialize get3DSpatialize() const { return mSpatialize; }

    void setResamplerIndex(ALsizei index);
    ALsizei getResamplerIndex() const { return mResampler; }

    void setAirAbsorptionFactor(ALfloat factor);
    ALfloat getAirAbsorptionFactor() const { return mAirAbsorptionFactor; }

    void setGainAuto(bool directhf, bool send, bool sendhf);
    std::tuple<bool,bool,bool> getGainAuto() const
    { return std::make_tuple(mDryGainHFAuto, mWetGainAuto, mWetGainHFAuto); }

    void setDirectFilter(const FilterParams &filter);
    void setSendFilter(ALuint send, const FilterParams &filter);
    void setAuxiliarySend(AuxiliaryEffectSlot slot, ALuint send);
    void setAuxiliarySendFilter(AuxiliaryEffectSlot slot, ALuint send, const FilterParams &filter);

    void update();

    void release();
};

} // namespace alure

#endif /* SOURCE_H */
