#ifndef SOURCE_H
#define SOURCE_H

#include "alure2.h"

#include <limits>

#include "al.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALContext;
class ALBuffer;
class ALBufferStream;

class ALSource : public Source {
    ALContext *const mContext;
    ALuint mId;

    ALBuffer *mBuffer;
    ALBufferStream *mStream;

    bool mLooping;
    bool mPaused;
    ALuint mOffset;
    ALfloat mPitch;
    ALfloat mGain;
    ALfloat mMinGain, mMaxGain;
    ALfloat mRefDist, mMaxDist;
    ALfloat mPosition[3];
    ALfloat mVelocity[3];
    ALfloat mDirection[3];
    ALfloat mConeInnerAngle, mConeOuterAngle;
    ALfloat mConeOuterGain;
    ALfloat mRolloffFactor;
    ALfloat mDopplerFactor;

    void resetProperties()
    {
        mLooping = false;
        mPaused = false;
        mOffset = 0;
        mPitch = 1.0f;
        mGain = 1.0f;
        mMinGain = 0.0f;
        mMaxGain = 1.0f;
        mRefDist = 1.0f;
        mMaxDist = std::numeric_limits<float>::max();
        mPosition[0] = mPosition[1] = mPosition[2] = 0.0f;
        mVelocity[0] = mVelocity[1] = mVelocity[2] = 0.0f;
        mDirection[0] = mDirection[1] = mDirection[2] = 0.0f;
        mConeInnerAngle = 360.0f;
        mConeOuterAngle = 360.0f;
        mConeOuterGain = 0.0f;
        mRolloffFactor = 1.0f;
        mDopplerFactor = 1.0f;
    }

    void applyProperties(bool looping, ALuint offset)
    {
        alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_SAMPLE_OFFSET, offset);
        alSourcef(mId, AL_PITCH, mPitch);
        alSourcef(mId, AL_GAIN, mGain);
        alSourcef(mId, AL_MIN_GAIN, mMinGain);
        alSourcef(mId, AL_MAX_GAIN, mMaxGain);
        alSourcef(mId, AL_REFERENCE_DISTANCE, mRefDist);
        alSourcef(mId, AL_MAX_DISTANCE, mMaxDist);
        alSourcefv(mId, AL_POSITION, mPosition);
        alSourcefv(mId, AL_VELOCITY, mVelocity);
        alSourcefv(mId, AL_DIRECTION, mDirection);
        alSourcef(mId, AL_CONE_INNER_ANGLE, mConeInnerAngle);
        alSourcef(mId, AL_CONE_OUTER_ANGLE, mConeOuterAngle);
        alSourcef(mId, AL_CONE_OUTER_GAIN, mConeOuterGain);
        alSourcef(mId, AL_ROLLOFF_FACTOR, mRolloffFactor);
        alSourcef(mId, AL_DOPPLER_FACTOR, mDopplerFactor);
    }

public:
    ALSource(ALContext *context)
      : mContext(context), mId(0), mBuffer(0), mStream(0)
    {
        resetProperties();
    }

    void updateNoCtxCheck();
    void finalize();

    virtual void play(Buffer *buffer) final;
    virtual void play(Decoder *decoder, ALuint updatelen, ALuint queuesize) final;
    virtual void stop() final;
    virtual void pause() final;
    virtual void resume() final;

    virtual bool isPlaying() const final;
    virtual bool isPaused() const final;

    virtual void setOffset(ALuint offset) final;
    virtual ALuint getOffset(uint64_t *latency=0) const final;

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

    virtual void setConeAngles(ALfloat inner, ALfloat outer) final;
    virtual void setOuterConeGain(ALfloat gain) final;

    virtual void setRolloffFactor(ALfloat factor) final;

    virtual void setDopplerFactor(ALfloat factor) final;

    virtual void update() final;
};

} // namespace alure

#endif /* SOURCE_H */
