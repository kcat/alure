#ifndef SOURCE_H
#define SOURCE_H

#include "alure2.h"

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
    ALfloat mGain;
    ALfloat mPosition[3];
    ALfloat mVelocity[3];
    ALfloat mDirection[3];
    ALfloat mRolloffFactor;
    ALfloat mDopplerFactor;

    void resetProperties()
    {
        mLooping = false;
        mGain = 1.0f;
        mPosition[0] = mPosition[1] = mPosition[2] = 0.0f;
        mVelocity[0] = mVelocity[1] = mVelocity[2] = 0.0f;
        mDirection[0] = mDirection[1] = mDirection[2] = 0.0f;
        mRolloffFactor = 1.0f;
        mDopplerFactor = 1.0f;
    }

    void applyProperties(bool looping)
    {
        alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
        alSourcef(mId, AL_GAIN, mGain);
        alSourcefv(mId, AL_POSITION, mPosition);
        alSourcefv(mId, AL_VELOCITY, mVelocity);
        alSourcefv(mId, AL_DIRECTION, mDirection);
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

    virtual void setLooping(bool looping) final;
    virtual bool getLooping() const final;

    virtual void play(Buffer *buffer) final;
    virtual void play(Decoder *decoder, ALuint updatelen, ALuint queuesize) final;
    virtual void stop() final;
    virtual void pause() final;
    virtual void resume() final;

    virtual bool isPlaying() const final;
    virtual bool isPaused() const final;

    virtual ALuint getOffset() const final;

    virtual void setGain(ALfloat gain) final;

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setPosition(const ALfloat *pos) final;

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setVelocity(const ALfloat *vel) final;

    virtual void setDirection(ALfloat x, ALfloat y, ALfloat z) final;
    virtual void setDirection(const ALfloat *dir) final;

    virtual void setRolloffFactor(ALfloat factor) final;

    virtual void setDopplerFactor(ALfloat factor) final;

    virtual void update() final;
};

} // namespace alure

#endif /* SOURCE_H */
