
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

    bool hasMoreData() const { return mDone; }
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

    if(mId == 0)
        mId = mContext->getSourceId();
    else
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
    }

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

    if(mId == 0)
        mId = mContext->getSourceId();
    else
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
    }

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
        else
        {
            ALint state = -1;
            alGetSourcei(mId, AL_SOURCE_STATE, &state);
            if(state != AL_PLAYING && state != AL_PAUSED)
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

ALuint ALSource::getOffset() const
{
    CheckContext(mContext);
    if(mId == 0)
        return 0;

    if(mStream)
    {
        ALint queued = 0, state = -1, srcpos = 0;
        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
        alGetSourcei(mId, AL_SOURCE_STATE, &state);

        ALuint pos = mStream->getPosition();
        if(state == AL_PLAYING || state == AL_PAUSED)
        {
            ALuint queuelen = queued * mStream->getUpdateLength();
            if(pos >= queuelen)
                pos -= queuelen;
            else
            {
                if(mLooping)
                {
                    ALuint streamlen = mStream->getLength();
                    if(streamlen+pos >= queuelen)
                        pos = streamlen + pos - queuelen;
                    else
                        pos = 0;
                }
                else
                    pos = 0;
            }
            pos += srcpos;
        }

        return pos;
    }

    ALint srcpos = 0;
    alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
    return srcpos;
}

}
