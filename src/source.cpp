
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

    ALenum getFormat() const { return mFormat; }
    ALuint getFrequency() const { return mFrequency; }

    ALuint getNumUpdates() const { return mNumUpdates; }

    void prepare()
    {
        ALuint srate;
        SampleConfig chans;
        SampleType type;
        mDecoder->getFormat(&srate, &chans, &type);

        mFormat = GetFormat(chans, type);
        mFrequency = srate;
        mFrameSize = FramesToBytes(1, chans, type);

        mData.resize(mUpdateLen * mFrameSize);
        mSilence = (type == SampleType_UInt8) ? 0x80 : 0x00;
    }

    const std::vector<ALbyte> *getNextData()
    {
        if(mDone) return 0;
        ALuint frames = mDecoder->read(&mData[0], mUpdateLen);
        if(frames < mUpdateLen)
        {
            mDone = true;
            memset(&mData[frames*mFrameSize], mSilence, (mUpdateLen-frames)*mFrameSize);
        }
        return (frames > 0) ? &mData : 0;
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

    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;

    delete mStream;
    mStream = 0;
}


void ALSource::play(Buffer *buffer, float volume)
{
    ALBuffer *albuf = dynamic_cast<ALBuffer*>(buffer);
    if(!albuf) throw std::runtime_error("Buffer is not valid");
    if(volume < 0.0f || volume > 1.0f)
        throw std::runtime_error("Volume out of range");
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

    alSourcei(mId, AL_BUFFER, mBuffer->getId());
    alSourcef(mId, AL_GAIN, volume);
    alSourcePlay(mId);
}

void ALSource::play(Decoder *decoder, ALuint updatelen, ALuint queuesize, float volume)
{
    if(volume < 0.0f || volume > 1.0f)
        throw std::runtime_error("Volume out of range");
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

    for(ALuint i = 0;i < mStream->getNumUpdates();i++)
    {
        const std::vector<ALbyte> *data = mStream->getNextData();
        if(!data) break;

        ALuint buf = 0;
        alGenBuffers(1, &buf);
        alBufferData(buf, mStream->getFormat(), &(*data)[0], data->size(), mStream->getFrequency());
        alSourceQueueBuffers(mId, 1, &buf);
    }
    alSourcef(mId, AL_GAIN, volume);
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
            const std::vector<ALbyte> *data = mStream->getNextData();
            if(!data) break;

            ALuint buf = 0;
            alGenBuffers(1, &buf);
            alBufferData(buf, mStream->getFormat(), &(*data)[0], data->size(), mStream->getFrequency());
            alSourceQueueBuffers(mId, 1, &buf);
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
