
#include "source.h"

#include <cstring>

#include <stdexcept>
#include <memory>

#include "al.h"

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

    bool mDone;

public:
    ALBufferStream(Decoder *decoder, ALuint updatelen, ALuint numupdates)
      : mDecoder(decoder), mUpdateLen(updatelen), mNumUpdates(numupdates),
        mFormat(AL_NONE), mFrequency(0), mFrameSize(0), mSilence(0),
        mDone(false)
    { }

    ALuint getNumUpdates() const { return mNumUpdates; }

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
    }

    bool hasMoreData() const { return mDone; }
    bool streamMoreData(ALuint srcid, bool loop)
    {
        if(mDone) return false;
        ALuint frames = mDecoder->read(&mData[0], mUpdateLen);
        if(frames < mUpdateLen)
        {
            if(loop)
            {
                do {
                    mDecoder->seek(0);
                    frames += mDecoder->read(&mData[frames*mFrameSize], mUpdateLen-frames);
                } while(frames < mUpdateLen);
            }
            else
            {
                mDone = true;
                if(frames == 0)
                    return false;
                memset(&mData[frames*mFrameSize], mSilence, (mUpdateLen-frames)*mFrameSize);
            }
        }

        ALuint buf = 0;
        alGenBuffers(1, &buf);
        alBufferData(buf, mFormat, &mData[0], mData.size(), mFrequency);
        alSourceQueueBuffers(srcid, 1, &buf);
        return true;
    }
};


void ALSource::reset()
{
    if(mId == 0)
        return;
    alSourceStop(mId);
    if(mStream)
    {
        ALint processed = 0;
        alGetSourcei(mId, AL_BUFFERS_PROCESSED, &processed);
        if(processed > 0)
        {
            ALuint bufs[processed];
            alSourceUnqueueBuffers(mId, processed, bufs);
            alDeleteBuffers(processed, bufs);
        }
    }
    alSourcei(mId, AL_BUFFER, 0);
}

void ALSource::finalize()
{
    CheckContext(mContext);

    reset();

    if(mId != 0)
        mContext->insertSourceId(mId);
    mId = 0;

    mContext->freeSource(this);

    mLooping = false;

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = 0;
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


void ALSource::play(Buffer *buffer)
{
    ALBuffer *albuf = dynamic_cast<ALBuffer*>(buffer);
    if(!albuf) throw std::runtime_error("Buffer is not valid");
    CheckContext(mContext);
    CheckContextDevice(albuf->getDevice());

    reset();
    if(mId == 0)
        mId = mContext->getSourceId();

    delete mStream;
    mStream = 0;

    albuf->addRef();
    if(mBuffer)
        mBuffer->decRef();
    mBuffer = albuf;

    alSourcei(mId, AL_LOOPING, mLooping ? AL_TRUE : AL_FALSE);

    alSourcei(mId, AL_BUFFER, mBuffer->getId());
    alSourcePlay(mId);
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

    reset();
    if(mId == 0)
        mId = mContext->getSourceId();

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = stream.release();

    alSourcei(mId, AL_LOOPING, AL_FALSE);

    for(ALuint i = 0;i < mStream->getNumUpdates();i++)
    {
        if(!mStream->streamMoreData(mId, mLooping))
            break;
    }
    alSourcePlay(mId);
}


void ALSource::stop()
{
    CheckContext(mContext);

    reset();

    if(mId != 0)
        mContext->insertSourceId(mId);
    mId = 0;

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = 0;
}

bool ALSource::isPlaying() const
{
    CheckContext(mContext);
    if(mId == 0) return false;

    if(mStream)
    {
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        if(state == -1)
            throw std::runtime_error("Source state error");
        return state == AL_PLAYING || (state != AL_PAUSED && mStream->hasMoreData());
    }

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");
    return state == AL_PLAYING;
}

void ALSource::update()
{
    CheckContext(mContext);
    if(mId == 0) return;

    if(mStream)
    {
        ALint processed, state;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        alGetSourcei(mId, AL_BUFFERS_PROCESSED, &processed);
        if(processed > 0)
        {
            ALuint bufs[processed];
            alSourceUnqueueBuffers(mId, processed, bufs);
            alDeleteBuffers(processed, bufs);
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
        else if(state != AL_PLAYING && state != AL_PAUSED)
            alSourcePlay(mId);
    }
    else
    {
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        if(state != AL_PLAYING && state != AL_PAUSED)
            stop();
    }
}

}
