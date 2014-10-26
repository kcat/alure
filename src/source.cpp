
#include "source.h"

#include <cstring>

#include <stdexcept>
#include <memory>

#include "al.h"
#include "alext.h"

#include "context.h"
#include "buffer.h"

namespace alure
{

class ALBufferStream {
    Decoder *mDecoder;

    ALuint mUpdateLen;
    ALuint mNumUpdates;

    ALenum mFormat;
    ALuint mFrequency;
    ALuint mFrameSize;

    std::vector<ALbyte> mData;
    ALuint mSilence;

    std::vector<ALuint> mBufferIds;
    ALuint mCurrentIdx;

    bool mDone;

public:
    ALBufferStream(Decoder *decoder, ALuint updatelen, ALuint numupdates)
      : mDecoder(decoder), mUpdateLen(updatelen), mNumUpdates(numupdates),
        mFormat(AL_NONE), mFrequency(0), mFrameSize(0), mSilence(0),
        mCurrentIdx(0), mDone(false)
    { }
    virtual ~ALBufferStream()
    {
        if(!mBufferIds.empty())
        {
            alDeleteBuffers(mBufferIds.size(), &mBufferIds[0]);
            mBufferIds.clear();
        }
    }

    ALuint getLength() const { return mDecoder->getLength(); }
    ALuint getPosition() const { return mDecoder->getPosition(); }

    ALuint getNumUpdates() const { return mNumUpdates; }
    ALuint getUpdateLength() const { return mUpdateLen; }

    bool seek(ALuint pos)
    {
        if(!mDecoder->seek(pos))
            return false;
        mDone = false;
        return true;
    }

    void prepare()
    {
        ALuint srate = mDecoder->getFrequency();
        SampleConfig chans = mDecoder->getSampleConfig();
        SampleType type = mDecoder->getSampleType();

        mFormat = GetFormat(chans, type);
        mFrequency = srate;
        mFrameSize = FramesToBytes(1, chans, type);

        mData.resize(mUpdateLen * mFrameSize);
        mSilence = (type == SampleType_UInt8) ? 0x80 : 0x00;

        mBufferIds.assign(mNumUpdates, 0);
        alGenBuffers(mBufferIds.size(), &mBufferIds[0]);
    }

    bool hasMoreData() const { return !mDone; }
    bool streamMoreData(ALuint srcid, bool loop)
    {
        if(mDone) return false;
        ALuint frames = mDecoder->read(&mData[0], mUpdateLen);
        if(loop && frames < mUpdateLen)
        {
            do {
                if(!mDecoder->seek(0)) break;
                ALuint got = mDecoder->read(&mData[frames*mFrameSize], mUpdateLen-frames);
                if(got == 0) break;
                frames += got;
            } while(frames < mUpdateLen);
        }
        if(frames < mUpdateLen)
        {
            mDone = true;
            if(frames == 0)
                return false;
            memset(&mData[frames*mFrameSize], mSilence, (mUpdateLen-frames)*mFrameSize);
        }

        alBufferData(mBufferIds[mCurrentIdx], mFormat, &mData[0], mData.size(), mFrequency);
        alSourceQueueBuffers(srcid, 1, &mBufferIds[mCurrentIdx]);
        mCurrentIdx = (mCurrentIdx+1) % mBufferIds.size();
        return true;
    }
};


void ALSource::finalize()
{
    CheckContext(mContext);

    if(mId != 0)
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        mContext->insertSourceId(mId);
        mId = 0;
    }

    mContext->freeSource(this);

    resetProperties();

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = 0;
}


void ALSource::play(Buffer *buffer)
{
    ALBuffer *albuf = dynamic_cast<ALBuffer*>(buffer);
    if(!albuf) throw std::runtime_error("Buffer is not valid");
    CheckContext(mContext);
    CheckContextDevice(albuf->getDevice());

    if(mId == 0)
    {
        mId = mContext->getSourceId();
        applyProperties(mLooping, mOffset);
    }
    else
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        alSourcei(mId, AL_LOOPING, mLooping ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_SAMPLE_OFFSET, mOffset);
    }
    mOffset = 0;

    delete mStream;
    mStream = 0;

    albuf->addRef();
    if(mBuffer)
        mBuffer->decRef();
    mBuffer = albuf;

    alSourcei(mId, AL_BUFFER, mBuffer->getId());
    alSourcePlay(mId);
    mPaused = false;
}

void ALSource::play(Decoder *decoder, ALuint updatelen, ALuint queuesize)
{
    if(updatelen < 64)
        throw std::runtime_error("Update length out of range");
    if(queuesize < 2)
        throw std::runtime_error("Queue size out of range");
    CheckContext(mContext);

    std::auto_ptr<ALBufferStream> stream(new ALBufferStream(decoder, updatelen, queuesize));
    stream->prepare();

    if(mId == 0)
    {
        mId = mContext->getSourceId();
        applyProperties(false, 0);
    }
    else
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        alSourcei(mId, AL_LOOPING, AL_FALSE);
        alSourcei(mId, AL_SAMPLE_OFFSET, 0);
    }

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = stream.release();

    mStream->seek(mOffset);
    mOffset = 0;

    for(ALuint i = 0;i < mStream->getNumUpdates();i++)
    {
        if(!mStream->streamMoreData(mId, mLooping))
            break;
    }
    alSourcePlay(mId);
    mPaused = false;
}


void ALSource::stop()
{
    CheckContext(mContext);

    if(mId != 0)
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        mContext->insertSourceId(mId);
        mId = 0;
    }

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = 0;

    mPaused = false;
}


void ALSource::pause()
{
    CheckContext(mContext);
    if(mId != 0)
    {
        alSourcePause(mId);
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        mPaused = (state == AL_PAUSED);
    }
}

void ALSource::resume()
{
    CheckContext(mContext);
    if(!mPaused) return;

    if(mId != 0)
    {
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        if(state == AL_PAUSED)
            alSourcePlay(mId);
    }
    mPaused = false;
}


bool ALSource::isPlaying() const
{
    CheckContext(mContext);
    if(mId == 0) return false;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");

    return state == AL_PLAYING || (mStream && !mPaused && mStream->hasMoreData());
}

bool ALSource::isPaused() const
{
    CheckContext(mContext);
    if(mId == 0) return false;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");

    return state == AL_PAUSED || mPaused;
}


void ALSource::update()
{
    CheckContext(mContext);
    updateNoCtxCheck();
}

void ALSource::updateNoCtxCheck()
{
    if(mId == 0)
        return;

    if(mStream)
    {
        ALint processed;
        alGetSourcei(mId, AL_BUFFERS_PROCESSED, &processed);
        if(processed > 0)
        {
            ALuint bufs[processed];
            alSourceUnqueueBuffers(mId, processed, bufs);
        }

        ALint queued;
        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        for(;(ALuint)queued < mStream->getNumUpdates();queued++)
        {
            if(!mStream->streamMoreData(mId, mLooping))
                break;
        }

        if(queued == 0)
            stop();
        else if(!mPaused)
        {
            ALint state = -1;
            alGetSourcei(mId, AL_SOURCE_STATE, &state);
            if(state != AL_PLAYING)
            {
                alGetSourcei(mId, AL_BUFFERS_PROCESSED, &processed);
                if(processed > 0)
                {
                    ALuint bufs[processed];
                    alSourceUnqueueBuffers(mId, processed, bufs);
                }

                alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
                for(;(ALuint)queued < mStream->getNumUpdates();queued++)
                {
                    if(!mStream->streamMoreData(mId, mLooping))
                        break;
                }
                alSourcePlay(mId);
            }
        }
    }
    else
    {
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        if(state != AL_PLAYING && state != AL_PAUSED)
            stop();
    }
}


void ALSource::setOffset(ALuint offset)
{
    CheckContext(mContext);
    if(mId == 0)
    {
        mOffset = offset;
        return;
    }

    if(!mStream)
        alSourcei(mId, AL_SAMPLE_OFFSET, offset);
    else
    {
        if(!mStream->seek(offset))
            throw std::runtime_error("Failed to seek to offset");
        alSourceStop(mId);
    }
}

ALuint ALSource::getOffset(uint64_t *latency) const
{
    CheckContext(mContext);
    if(mId == 0)
    {
        if(latency)
            *latency = 0;
        return 0;
    }

    if(mStream)
    {
        ALint queued = 0, state = -1, srcpos = 0;
        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        if(latency && mContext->hasExtension(SOFT_source_latency))
        {
            ALint64SOFT val[2];
            mContext->alGetSourcei64vSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
            srcpos = val[0]>>32;
            *latency = val[1];
        }
        else
        {
            alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
            if(latency) *latency = 0;
        }
        alGetSourcei(mId, AL_SOURCE_STATE, &state);

        ALuint pos = mStream->getPosition();
        if(state == AL_PLAYING || state == AL_PAUSED)
        {
            // The amount of samples in the queue waiting to play
            ALuint inqueue = queued*mStream->getUpdateLength() - srcpos;

            if(pos >= inqueue)
                pos -= inqueue;
            else if(!mLooping)
            {
                // A non-looping stream should never have more samples queued
                // than have been read...
                pos = 0;
            }
            else
            {
                ALuint streamlen = mStream->getLength();
                if(streamlen == 0)
                {
                    // A looped stream that doesn't know its own length?
                    pos = 0;
                }
                else while((ALint)pos < 0)
                    pos += streamlen;
            }
        }

        return pos;
    }

    ALint srcpos = 0;
    if(latency && mContext->hasExtension(SOFT_source_latency))
    {
        ALint64SOFT val[2];
        mContext->alGetSourcei64vSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
        srcpos = val[0]>>32;
        *latency = val[1];
    }
    else
    {
        alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
        if(latency) *latency = 0;
    }
    return srcpos;
}


void ALSource::setLooping(bool looping)
{
    CheckContext(mContext);

    if(mId && !mStream)
        alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    mLooping = looping;
}

bool ALSource::getLooping() const
{
    return mLooping;
}


void ALSource::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::runtime_error("Pitch out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_PITCH, pitch);
    mPitch = pitch;
}


void ALSource::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_GAIN, gain);
    mGain = gain;
}

void ALSource::setGainRange(ALfloat mingain, ALfloat maxgain)
{
    if(!(mingain >= 0.0f && maxgain <= 1.0f && maxgain >= mingain))
        throw std::runtime_error("Gain range out of range");
    CheckContext(mContext);
    if(mId != 0)
    {
        alSourcef(mId, AL_MIN_GAIN, mingain);
        alSourcef(mId, AL_MAX_GAIN, maxgain);
    }
    mMinGain = mingain;
    mMaxGain = maxgain;
}


void ALSource::setDistanceRange(ALfloat refdist, ALfloat maxdist)
{
    if(!(refdist >= 0.0f && maxdist <= std::numeric_limits<float>::max() && refdist <= maxdist))
        throw std::runtime_error("Distance range out of range");
    CheckContext(mContext);
    if(mId != 0)
    {
        alSourcef(mId, AL_REFERENCE_DISTANCE, refdist);
        alSourcef(mId, AL_MAX_DISTANCE, maxdist);
    }
    mRefDist = refdist;
    mMaxDist = maxdist;
}


void ALSource::setPosition(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    if(mId != 0)
        alSource3f(mId, AL_POSITION, x, y, z);
    mPosition[0] = x;
    mPosition[1] = y;
    mPosition[2] = z;
}

void ALSource::setPosition(const ALfloat *pos)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcefv(mId, AL_POSITION, pos);
    mPosition[0] = pos[0];
    mPosition[1] = pos[1];
    mPosition[2] = pos[2];
}

void ALSource::setVelocity(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    if(mId != 0)
        alSource3f(mId, AL_VELOCITY, x, y, z);
    mVelocity[0] = x;
    mVelocity[1] = y;
    mVelocity[2] = z;
}

void ALSource::setVelocity(const ALfloat *vel)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcefv(mId, AL_VELOCITY, vel);
    mVelocity[0] = vel[0];
    mVelocity[1] = vel[1];
    mVelocity[2] = vel[2];
}

void ALSource::setDirection(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    if(mId != 0)
        alSource3f(mId, AL_DIRECTION, x, y, z);
    mDirection[0] = x;
    mDirection[1] = y;
    mDirection[2] = z;
}

void ALSource::setDirection(const ALfloat *dir)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcefv(mId, AL_DIRECTION, dir);
    mDirection[0] = dir[0];
    mDirection[1] = dir[1];
    mDirection[2] = dir[2];
}


void ALSource::setConeAngles(ALfloat inner, ALfloat outer)
{
    if(!(inner >= 0.0f && outer <= 360.0f && outer >= inner))
        throw std::runtime_error("Cone angles out of range");
    CheckContext(mContext);
    if(mId != 0)
    {
        alSourcef(mId, AL_CONE_INNER_ANGLE, inner);
        alSourcef(mId, AL_CONE_OUTER_ANGLE, outer);
    }
    mConeInnerAngle = inner;
    mConeOuterAngle = outer;
}

void ALSource::setOuterConeGain(ALfloat gain)
{
    if(!(gain >= 0.0f && gain <= 1.0f))
        throw std::runtime_error("Outer cone gain out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_CONE_OUTER_GAIN, gain);
    mConeOuterGain = gain;
}


void ALSource::setRolloffFactor(ALfloat factor)
{
    if(!(factor >= 0.0f))
        throw std::runtime_error("Doppler factor out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_ROLLOFF_FACTOR, factor);
    mRolloffFactor = factor;
}

void ALSource::setDopplerFactor(ALfloat factor)
{
    if(!(factor >= 0.0f && factor <= 1.0f))
        throw std::runtime_error("Doppler factor out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_DOPPLER_FACTOR, factor);
    mDopplerFactor = factor;
}

}
