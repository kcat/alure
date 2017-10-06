
#include "config.h"

#include "source.h"

#include <cstring>

#include <stdexcept>
#include <sstream>
#include <memory>
#include <limits>

#include "al.h"
#include "alext.h"

#include "context.h"
#include "buffer.h"
#include "auxeffectslot.h"
#include "sourcegroup.h"

namespace alure
{

class ALBufferStream {
    SharedPtr<Decoder> mDecoder;

    ALuint mUpdateLen;
    ALuint mNumUpdates;

    ALenum mFormat;
    ALuint mFrequency;
    ALuint mFrameSize;

    Vector<ALbyte> mData;
    ALbyte mSilence;

    Vector<ALuint> mBufferIds;
    ALuint mCurrentIdx;

    std::pair<uint64_t,uint64_t> mLoopPts;
    std::atomic<bool> mHasLooped;
    std::atomic<bool> mDone;

public:
    ALBufferStream(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint numupdates)
      : mDecoder(decoder), mUpdateLen(updatelen), mNumUpdates(numupdates),
        mFormat(AL_NONE), mFrequency(0), mFrameSize(0), mSilence(0),
        mCurrentIdx(0), mLoopPts{0,0}, mHasLooped(false), mDone(false)
    { }
    ~ALBufferStream()
    {
        if(!mBufferIds.empty())
        {
            alDeleteBuffers(mBufferIds.size(), &mBufferIds[0]);
            mBufferIds.clear();
        }
    }

    uint64_t getLength() const { return mDecoder->getLength(); }
    uint64_t getPosition() const { return mDecoder->getPosition(); }

    ALuint getNumUpdates() const { return mNumUpdates; }
    ALuint getUpdateLength() const { return mUpdateLen; }

    ALuint getFrequency() const { return mFrequency; }

    bool seek(uint64_t pos)
    {
        if(!mDecoder->seek(pos))
            return false;
        mHasLooped.store(false, std::memory_order_release);
        mDone.store(false, std::memory_order_release);
        return true;
    }

    void prepare()
    {
        ALuint srate = mDecoder->getFrequency();
        ChannelConfig chans = mDecoder->getChannelConfig();
        SampleType type = mDecoder->getSampleType();

        mLoopPts = mDecoder->getLoopPoints();
        if(mLoopPts.first >= mLoopPts.second)
        {
            mLoopPts.first = 0;
            mLoopPts.second = std::numeric_limits<uint64_t>::max();
        }

        mFrequency = srate;
        mFrameSize = FramesToBytes(1, chans, type);
        mFormat = GetFormat(chans, type);
        if(mFormat == AL_NONE)
        {
            std::stringstream sstr;
            sstr<< "Format not supported ("<<GetSampleTypeName(type)<<", "<<GetChannelConfigName(chans)<<")";
            throw std::runtime_error(sstr.str());
        }

        mData.resize(mUpdateLen * mFrameSize);
        if(type == SampleType::UInt8) mSilence = 0x80;
        else if(type == SampleType::Mulaw) mSilence = 0x7f;
        else mSilence = 0x00;

        mBufferIds.assign(mNumUpdates, 0);
        alGenBuffers(mBufferIds.size(), &mBufferIds[0]);
    }

    int64_t getLoopStart() const { return mLoopPts.first; }
    int64_t getLoopEnd() const { return mLoopPts.second; }

    bool hasLooped() const { return mHasLooped.load(std::memory_order_acquire); }
    bool hasMoreData() const { return !mDone.load(std::memory_order_acquire); }
    bool streamMoreData(ALuint srcid, bool loop)
    {
        if(mDone.load(std::memory_order_acquire))
            return false;

        ALuint frames;
        if(!loop)
            frames = mDecoder->read(&mData[0], mUpdateLen);
        else
        {
            ALuint len = mUpdateLen;
            uint64_t pos = mDecoder->getPosition();
            if(pos <= mLoopPts.second)
                len = std::min<uint64_t>(len, mLoopPts.second - pos);
            else
                loop = false;

            frames = mDecoder->read(&mData[0], len);
            if(frames < mUpdateLen && loop && pos+frames > 0)
            {
                if(pos+frames < mLoopPts.second)
                {
                    mLoopPts.second = pos+frames;
                    mLoopPts.first = std::min(mLoopPts.first, mLoopPts.second-1);
                }

                do {
                    if(!mDecoder->seek(mLoopPts.first))
                        break;
                    mHasLooped.store(true, std::memory_order_release);

                    len = std::min<uint64_t>(mUpdateLen-frames, mLoopPts.second-mLoopPts.first);
                    ALuint got = mDecoder->read(&mData[frames*mFrameSize], len);
                    if(got == 0) break;
                    frames += got;
                } while(frames < mUpdateLen);
            }
        }
        if(frames < mUpdateLen)
        {
            mDone.store(true, std::memory_order_release);
            if(frames == 0) return false;
            std::fill(mData.begin() + frames*mFrameSize, mData.end(), mSilence);
        }

        alBufferData(mBufferIds[mCurrentIdx], mFormat, &mData[0], mData.size(), mFrequency);
        alSourceQueueBuffers(srcid, 1, &mBufferIds[mCurrentIdx]);
        mCurrentIdx = (mCurrentIdx+1) % mBufferIds.size();
        return true;
    }
};


ALSource::ALSource(ALContext *context)
  : mContext(context), mId(0), mBuffer(0), mGroup(nullptr), mIsAsync(false),
    mDirectFilter(AL_FILTER_NULL)
{
    resetProperties();
}

ALSource::~ALSource()
{
}


void ALSource::resetProperties()
{
    if(mGroup)
        mGroup->removeSource(Source(this));
    mGroup = nullptr;

    mPaused.store(false, std::memory_order_release);
    mOffset = 0;
    mPitch = 1.0f;
    mGain = 1.0f;
    mMinGain = 0.0f;
    mMaxGain = 1.0f;
    mRefDist = 1.0f;
    mMaxDist = std::numeric_limits<float>::max();
    mPosition = Vector3(0.0f);
    mVelocity = Vector3(0.0f);
    mDirection = Vector3(0.0f);
    mOrientation[0] = Vector3(0.0f, 0.0f, -1.0f);
    mOrientation[1] = Vector3(0.0f, 1.0f,  0.0f);
    mConeInnerAngle = 360.0f;
    mConeOuterAngle = 360.0f;
    mConeOuterGain = 0.0f;
    mConeOuterGainHF = 1.0f;
    mRolloffFactor = 1.0f;
    mRoomRolloffFactor = 0.0f;
    mDopplerFactor = 1.0f;
    mAirAbsorptionFactor = 0.0f;
    mRadius = 0.0f;
    mStereoAngles[0] =  F_PI / 6.0f;
    mStereoAngles[1] = -F_PI / 6.0f;
    mSpatialize = Spatialize::Auto;
    mResampler = mContext->hasExtension(SOFT_source_resampler) ?
                 alGetInteger(AL_DEFAULT_RESAMPLER_SOFT) : 0;
    mLooping = false;
    mRelative = false;
    mDryGainHFAuto = true;
    mWetGainAuto = true;
    mWetGainHFAuto = true;
    if(mDirectFilter)
        mContext->alDeleteFilters(1, &mDirectFilter);
    mDirectFilter = 0;
    for(auto &i : mEffectSlots)
    {
        if(i.second.mSlot)
            i.second.mSlot->removeSourceSend(Source(this), i.first);
        if(i.second.mFilter)
            mContext->alDeleteFilters(1, &i.second.mFilter);
    }
    mEffectSlots.clear();

    mPriority = 0;
}

void ALSource::applyProperties(bool looping, ALuint offset) const
{
    alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    alSourcei(mId, AL_SAMPLE_OFFSET, offset);
    if(mGroup)
    {
        alSourcef(mId, AL_PITCH, mPitch * mGroup->getAppliedPitch());
        alSourcef(mId, AL_GAIN, mGain * mGroup->getAppliedGain());
    }
    else
    {
        alSourcef(mId, AL_PITCH, mPitch);
        alSourcef(mId, AL_GAIN, mGain);
    }
    alSourcef(mId, AL_MIN_GAIN, mMinGain);
    alSourcef(mId, AL_MAX_GAIN, mMaxGain);
    alSourcef(mId, AL_REFERENCE_DISTANCE, mRefDist);
    alSourcef(mId, AL_MAX_DISTANCE, mMaxDist);
    alSourcefv(mId, AL_POSITION, mPosition.getPtr());
    alSourcefv(mId, AL_VELOCITY, mVelocity.getPtr());
    alSourcefv(mId, AL_DIRECTION, mDirection.getPtr());
    if(mContext->hasExtension(EXT_BFORMAT))
        alSourcefv(mId, AL_ORIENTATION, &mOrientation[0][0]);
    alSourcef(mId, AL_CONE_INNER_ANGLE, mConeInnerAngle);
    alSourcef(mId, AL_CONE_OUTER_ANGLE, mConeOuterAngle);
    alSourcef(mId, AL_CONE_OUTER_GAIN, mConeOuterGain);
    alSourcef(mId, AL_ROLLOFF_FACTOR, mRolloffFactor);
    alSourcef(mId, AL_DOPPLER_FACTOR, mDopplerFactor);
    if(mContext->hasExtension(EXT_SOURCE_RADIUS))
        alSourcef(mId, AL_SOURCE_RADIUS, mRadius);
    if(mContext->hasExtension(EXT_STEREO_ANGLES))
        alSourcefv(mId, AL_STEREO_ANGLES, mStereoAngles);
    if(mContext->hasExtension(SOFT_source_spatialize))
        alSourcei(mId, AL_SOURCE_SPATIALIZE_SOFT, (ALint)mSpatialize);
    if(mContext->hasExtension(SOFT_source_resampler))
        alSourcei(mId, AL_SOURCE_RESAMPLER_SOFT, mResampler);
    alSourcei(mId, AL_SOURCE_RELATIVE, mRelative ? AL_TRUE : AL_FALSE);
    if(mContext->hasExtension(EXT_EFX))
    {
        alSourcef(mId, AL_CONE_OUTER_GAINHF, mConeOuterGainHF);
        alSourcef(mId, AL_ROOM_ROLLOFF_FACTOR, mRoomRolloffFactor);
        alSourcef(mId, AL_AIR_ABSORPTION_FACTOR, mAirAbsorptionFactor);
        alSourcei(mId, AL_DIRECT_FILTER_GAINHF_AUTO, mDryGainHFAuto ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO, mWetGainAuto ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO, mWetGainHFAuto ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_DIRECT_FILTER, mDirectFilter);
        for(const auto &i : mEffectSlots)
        {
            ALuint slotid = (i.second.mSlot ? i.second.mSlot->getId() : 0);
            alSource3i(mId, AL_AUXILIARY_SEND_FILTER, slotid, i.first, i.second.mFilter);
        }
    }
}


void ALSource::setGroup(ALSourceGroup *group)
{
    if(mGroup)
        mGroup->removeSource(Source(this));
    mGroup = group;
    groupUpdate();
}

void ALSource::unsetGroup()
{
    mGroup = nullptr;
    groupUpdate();
}

void ALSource::groupUpdate()
{
    if(mId)
    {
        if(mGroup)
        {
            alSourcef(mId, AL_PITCH, mPitch * mGroup->getAppliedPitch());
            alSourcef(mId, AL_GAIN, mGain * mGroup->getAppliedGain());
        }
        else
        {
            alSourcef(mId, AL_PITCH, mPitch);
            alSourcef(mId, AL_GAIN, mGain);
        }
    }
}

void ALSource::groupPropUpdate(ALfloat gain, ALfloat pitch)
{
    if(mId)
    {
        alSourcef(mId, AL_PITCH, mPitch * pitch);
        alSourcef(mId, AL_GAIN, mGain * gain);
    }
}


void ALSource::play(Buffer buffer)
{
    ALBuffer *albuf = buffer.getHandle();
    if(!albuf) throw std::runtime_error("Buffer is not valid");
    CheckContext(mContext);
    CheckContext(albuf->getContext());

    if(!albuf->isReady())
        throw std::runtime_error("Buffer is not ready");

    if(mIsAsync.load(std::memory_order_acquire))
    {
        mContext->removeStream(this);
        mIsAsync.store(false, std::memory_order_release);
    }

    if(mId == 0)
    {
        mId = mContext->getSourceId(mPriority);
        applyProperties(mLooping, (ALuint)std::min<uint64_t>(mOffset, std::numeric_limits<ALint>::max()));
    }
    else
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        alSourcei(mId, AL_LOOPING, mLooping ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_SAMPLE_OFFSET, (ALuint)std::min<uint64_t>(mOffset, std::numeric_limits<ALint>::max()));
    }
    mOffset = 0;

    mStream.reset();

    if(mBuffer)
        mBuffer->removeSource(Source(this));
    mBuffer = albuf;
    mBuffer->addSource(Source(this));

    alSourcei(mId, AL_BUFFER, mBuffer->getId());
    alSourcePlay(mId);
    mPaused.store(false, std::memory_order_release);
}

void ALSource::play(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint queuesize)
{
    if(updatelen < 64)
        throw std::runtime_error("Update length out of range");
    if(queuesize < 2)
        throw std::runtime_error("Queue size out of range");
    CheckContext(mContext);

    auto stream = MakeUnique<ALBufferStream>(decoder, updatelen, queuesize);
    stream->prepare();

    if(mIsAsync.load(std::memory_order_acquire))
    {
        mContext->removeStream(this);
        mIsAsync.store(false, std::memory_order_release);
    }

    if(mId == 0)
    {
        mId = mContext->getSourceId(mPriority);
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
        mBuffer->removeSource(Source(this));
    mBuffer = 0;

    mStream = std::move(stream);

    mStream->seek(mOffset);
    mOffset = 0;

    for(ALuint i = 0;i < mStream->getNumUpdates();i++)
    {
        if(!mStream->streamMoreData(mId, mLooping))
            break;
    }
    alSourcePlay(mId);
    mPaused.store(false, std::memory_order_release);

    mContext->addStream(this);
    mIsAsync.store(true, std::memory_order_release);
}


void ALSource::makeStopped()
{
    if(mIsAsync.load(std::memory_order_acquire))
    {
        mContext->removeStreamNoLock(this);
        mIsAsync.store(false, std::memory_order_release);
    }

    if(mId != 0)
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        if(mContext->hasExtension(EXT_EFX))
        {
            alSourcei(mId, AL_DIRECT_FILTER, AL_FILTER_NULL);
            for(auto &i : mEffectSlots)
                alSource3i(mId, AL_AUXILIARY_SEND_FILTER, 0, i.first, AL_FILTER_NULL);
        }
        mContext->insertSourceId(mId);
        mId = 0;
    }

    if(mBuffer)
        mBuffer->removeSource(Source(this));
    mBuffer = 0;

    mStream.reset();

    mPaused.store(false, std::memory_order_release);
}

void ALSource::stop()
{
    CheckContext(mContext);
    makeStopped();
}


void ALSource::checkPaused()
{
    if(mPaused.load(std::memory_order_acquire) || mId == 0)
        return;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    // Streaming sources may be in a stopped state if underrun
    mPaused.store((state == AL_PAUSED) ||
                  (state == AL_STOPPED && mStream && mStream->hasMoreData()),
                  std::memory_order_release);
}

void ALSource::pause()
{
    CheckContext(mContext);
    if(mPaused.load(std::memory_order_acquire))
        return;

    if(mId != 0)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        alSourcePause(mId);
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        // Streaming sources may be in a stopped state if underrun
        mPaused.store((state == AL_PAUSED) ||
                      (state == AL_STOPPED && mStream && mStream->hasMoreData()),
                      std::memory_order_release);
    }
}

void ALSource::resume()
{
    CheckContext(mContext);
    if(!mPaused.load(std::memory_order_acquire))
        return;

    if(mId != 0)
        alSourcePlay(mId);
    mPaused.store(false, std::memory_order_release);
}


bool ALSource::isPlaying() const
{
    CheckContext(mContext);
    if(mId == 0) return false;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");

    return state == AL_PLAYING || (!mPaused.load(std::memory_order_acquire) &&
                                   mStream && mStream->hasMoreData());
}

bool ALSource::isPaused() const
{
    CheckContext(mContext);
    if(mId == 0) return false;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");

    return state == AL_PAUSED || mPaused.load(std::memory_order_acquire);
}


ALint ALSource::refillBufferStream()
{
    ALint processed;
    alGetSourcei(mId, AL_BUFFERS_PROCESSED, &processed);
    while(processed > 0)
    {
        ALuint buf;
        alSourceUnqueueBuffers(mId, 1, &buf);
        --processed;
    }

    ALint queued;
    alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
    for(;(ALuint)queued < mStream->getNumUpdates();queued++)
    {
        if(!mStream->streamMoreData(mId, mLooping))
            break;
    }

    return queued;
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
        if(!mIsAsync.load(std::memory_order_acquire))
        {
            stop();
            mContext->send(&MessageHandler::sourceStopped, Source(this));
        }
    }
    else
    {
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        if(state != AL_PLAYING && state != AL_PAUSED)
        {
            stop();
            mContext->send(&MessageHandler::sourceStopped, Source(this));
        }
    }
}

bool ALSource::updateAsync()
{
    std::lock_guard<std::mutex> lock(mMutex);

    ALint queued = refillBufferStream();
    if(queued == 0)
    {
        mIsAsync.store(false, std::memory_order_release);
        return false;
    }
    if(!mPaused.load(std::memory_order_acquire))
    {
        ALint state = -1;
        alGetSourcei(mId, AL_SOURCE_STATE, &state);
        if(state != AL_PLAYING)
            alSourcePlay(mId);
    }
    return true;
}


void ALSource::setPriority(ALuint priority)
{
    mPriority = priority;
}


void ALSource::setOffset(uint64_t offset)
{
    CheckContext(mContext);
    if(mId == 0)
    {
        mOffset = offset;
        return;
    }

    if(!mStream)
    {
        if(offset >= std::numeric_limits<ALint>::max())
            throw std::runtime_error("Offset out of range");
        alGetError();
        alSourcei(mId, AL_SAMPLE_OFFSET, (ALint)offset);
        if(alGetError() != AL_NO_ERROR)
            throw std::runtime_error("Offset out of range");
    }
    else
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if(!mStream->seek(offset))
            throw std::runtime_error("Failed to seek to offset");
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        ALint queued = refillBufferStream();
        if(queued > 0 && !mPaused)
            alSourcePlay(mId);
    }
}

std::pair<uint64_t,std::chrono::nanoseconds> ALSource::getSampleOffsetLatency() const
{
    CheckContext(mContext);
    if(mId == 0)
        return { 0, std::chrono::nanoseconds::zero() };

    if(mStream)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        ALint queued = 0, state = -1, srcpos = 0;
        std::chrono::nanoseconds latency(0);

        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        if(mContext->hasExtension(SOFT_source_latency))
        {
            ALint64SOFT val[2];
            mContext->alGetSourcei64vSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
            srcpos = val[0]>>32;
            latency = std::chrono::nanoseconds(val[1]);
        }
        else
            alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
        alGetSourcei(mId, AL_SOURCE_STATE, &state);

        int64_t streampos = mStream->getPosition();
        if(state != AL_STOPPED)
        {
            // The amount of samples in the queue waiting to play
            ALuint inqueue = queued*mStream->getUpdateLength() - srcpos;
            if(!mStream->hasLooped())
            {
                // A non-looped stream should never have more samples queued
                // than have been read...
                streampos = std::max<int64_t>(streampos, inqueue) - inqueue;
            }
            else
            {
                streampos -= inqueue;
                int64_t looplen = mStream->getLoopEnd() - mStream->getLoopStart();
                while(streampos < mStream->getLoopStart())
                    streampos += looplen;
            }
        }

        return { streampos, latency };
    }

    std::chrono::nanoseconds latency(0);
    ALint srcpos = 0;
    if(mContext->hasExtension(SOFT_source_latency))
    {
        ALint64SOFT val[2];
        mContext->alGetSourcei64vSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
        srcpos = val[0]>>32;
        latency = std::chrono::nanoseconds(val[1]);
    }
    else
        alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
    return { srcpos, latency };
}

std::pair<Seconds,Seconds> ALSource::getSecOffsetLatency() const
{
    CheckContext(mContext);
    if(mId == 0)
        return { Seconds::zero(), Seconds::zero() };

    if(mStream)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        ALint queued = 0, state = -1;
        ALdouble srcpos = 0;
        Seconds latency(0.0);

        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        if(mContext->hasExtension(SOFT_source_latency))
        {
            ALdouble val[2];
            mContext->alGetSourcedvSOFT(mId, AL_SEC_OFFSET_LATENCY_SOFT, val);
            srcpos = val[0];
            latency = Seconds(val[1]);
        }
        else
        {
            ALfloat f;
            alGetSourcef(mId, AL_SEC_OFFSET, &f);
            srcpos = f;
        }
        alGetSourcei(mId, AL_SOURCE_STATE, &state);

        ALdouble frac = 0.0;
        int64_t streampos = mStream->getPosition();
        if(state != AL_STOPPED)
        {
            ALdouble ipos;
            frac = std::modf(srcpos * mStream->getFrequency(), &ipos);

            // The amount of samples in the queue waiting to play
            ALuint inqueue = queued*mStream->getUpdateLength() - (ALuint)ipos;
            if(!mStream->hasLooped())
            {
                // A non-looped stream should never have more samples queued
                // than have been read...
                streampos = std::max<int64_t>(streampos, inqueue) - inqueue;
            }
            else
            {
                streampos -= inqueue;
                int64_t looplen = mStream->getLoopEnd() - mStream->getLoopStart();
                while(streampos < mStream->getLoopStart())
                    streampos += looplen;
            }
        }

        return { Seconds((streampos+frac) / mStream->getFrequency()), latency };
    }

    ALdouble srcpos = 0.0;
    Seconds latency(0.0);
    if(mContext->hasExtension(SOFT_source_latency))
    {
        ALdouble val[2];
        mContext->alGetSourcedvSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
        srcpos = val[0];
        latency = Seconds(val[1]);
    }
    else
    {
        ALfloat f;
        alGetSourcef(mId, AL_SEC_OFFSET, &f);
        srcpos = f;
    }
    return { Seconds(srcpos), latency };
}


void ALSource::setLooping(bool looping)
{
    CheckContext(mContext);

    if(mId && !mStream)
        alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    mLooping = looping;
}


void ALSource::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::runtime_error("Pitch out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_PITCH, pitch * (mGroup ? mGroup->getAppliedPitch() : 1.0f));
    mPitch = pitch;
}


void ALSource::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_GAIN, gain * (mGroup ? mGroup->getAppliedGain() : 1.0f));
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

void ALSource::setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2)
{
    CheckContext(mContext);
    if(mId != 0)
    {
        ALfloat ori[6] = { x1, y1, z1, x2, y2, z2 };
        if(mContext->hasExtension(EXT_BFORMAT))
            alSourcefv(mId, AL_ORIENTATION, ori);
        alSourcefv(mId, AL_DIRECTION, ori);
    }
    mDirection[0] = mOrientation[0][0] = x1;
    mDirection[1] = mOrientation[0][1] = y1;
    mDirection[2] = mOrientation[0][2] = z1;
    mOrientation[1][0] = x2;
    mOrientation[1][1] = y2;
    mOrientation[1][2] = z2;
}

void ALSource::setOrientation(const ALfloat *at, const ALfloat *up)
{
    CheckContext(mContext);
    if(mId != 0)
    {
        ALfloat ori[6] = { at[0], at[1], at[2], up[0], up[1], up[2] };
        if(mContext->hasExtension(EXT_BFORMAT))
            alSourcefv(mId, AL_ORIENTATION, ori);
        alSourcefv(mId, AL_DIRECTION, ori);
    }
    mDirection[0] = mOrientation[0][0] = at[0];
    mDirection[1] = mOrientation[0][1] = at[1];
    mDirection[2] = mOrientation[0][2] = at[2];
    mOrientation[1][0] = up[0];
    mOrientation[1][1] = up[1];
    mOrientation[1][2] = up[2];
}

void ALSource::setOrientation(const ALfloat *ori)
{
    CheckContext(mContext);
    if(mId != 0)
    {
        if(mContext->hasExtension(EXT_BFORMAT))
            alSourcefv(mId, AL_ORIENTATION, ori);
        alSourcefv(mId, AL_DIRECTION, ori);
    }
    mDirection[0] = mOrientation[0][0] = ori[0];
    mDirection[1] = mOrientation[0][1] = ori[1];
    mDirection[2] = mOrientation[0][2] = ori[2];
    mOrientation[1][0] = ori[3];
    mOrientation[1][1] = ori[4];
    mOrientation[1][2] = ori[5];
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

void ALSource::setOuterConeGains(ALfloat gain, ALfloat gainhf)
{
    if(!(gain >= 0.0f && gain <= 1.0f && gainhf >= 0.0f && gainhf <= 1.0f))
        throw std::runtime_error("Outer cone gain out of range");
    CheckContext(mContext);
    if(mId != 0)
    {
        alSourcef(mId, AL_CONE_OUTER_GAIN, gain);
        if(mContext->hasExtension(EXT_EFX))
            alSourcef(mId, AL_CONE_OUTER_GAINHF, gainhf);
    }
    mConeOuterGain = gain;
    mConeOuterGainHF = gainhf;
}


void ALSource::setRolloffFactors(ALfloat factor, ALfloat roomfactor)
{
    if(!(factor >= 0.0f && roomfactor >= 0.0f))
        throw std::runtime_error("Rolloff factor out of range");
    CheckContext(mContext);
    if(mId != 0)
    {
        alSourcef(mId, AL_ROLLOFF_FACTOR, factor);
        if(mContext->hasExtension(EXT_EFX))
            alSourcef(mId, AL_ROOM_ROLLOFF_FACTOR, roomfactor);
    }
    mRolloffFactor = factor;
    mRoomRolloffFactor = roomfactor;
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

void ALSource::setAirAbsorptionFactor(ALfloat factor)
{
    if(!(factor >= 0.0f && factor <= 10.0f))
        throw std::runtime_error("Absorption factor out of range");
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(EXT_EFX))
        alSourcef(mId, AL_AIR_ABSORPTION_FACTOR, factor);
    mAirAbsorptionFactor = factor;
}

void ALSource::setRadius(ALfloat radius)
{
    if(!(mRadius >= 0.0f))
        throw std::runtime_error("Radius out of range");
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(EXT_SOURCE_RADIUS))
        alSourcef(mId, AL_SOURCE_RADIUS, radius);
    mRadius = radius;
}

void ALSource::setStereoAngles(ALfloat leftAngle, ALfloat rightAngle)
{
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(EXT_STEREO_ANGLES))
    {
        ALfloat angles[2] = { leftAngle, rightAngle };
        alSourcefv(mId, AL_STEREO_ANGLES, angles);
    }
    mStereoAngles[0] = leftAngle;
    mStereoAngles[1] = rightAngle;
}

void ALSource::set3DSpatialize(Spatialize spatialize)
{
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(SOFT_source_spatialize))
        alSourcei(mId, AL_SOURCE_SPATIALIZE_SOFT, (ALint)spatialize);
    mSpatialize = spatialize;
}

void ALSource::setResamplerIndex(ALsizei index)
{
    if(index < 0)
        throw std::runtime_error("Resampler index out of range");
    index = std::min<ALsizei>(index, mContext->getAvailableResamplers().size());
    if(mId != 0 && mContext->hasExtension(SOFT_source_resampler))
        alSourcei(mId, AL_SOURCE_RESAMPLER_SOFT, index);
    mResampler = index;
}

void ALSource::setRelative(bool relative)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcei(mId, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);
    mRelative = relative;
}

void ALSource::setGainAuto(bool directhf, bool send, bool sendhf)
{
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(EXT_EFX))
    {
        alSourcei(mId, AL_DIRECT_FILTER_GAINHF_AUTO, directhf ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_AUXILIARY_SEND_FILTER_GAIN_AUTO, send ? AL_TRUE : AL_FALSE);
        alSourcei(mId, AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO, sendhf ? AL_TRUE : AL_FALSE);
    }
    mDryGainHFAuto = directhf;
    mWetGainAuto = send;
    mWetGainHFAuto = sendhf;
}


void ALSource::setFilterParams(ALuint &filterid, const FilterParams &params)
{
    if(!mContext->hasExtension(EXT_EFX))
        return;

    if(!(params.mGain < 1.0f || params.mGainHF < 1.0f || params.mGainLF < 1.0f))
    {
        if(filterid)
            mContext->alFilteri(filterid, AL_FILTER_TYPE, AL_FILTER_NULL);
        return;
    }

    alGetError();
    if(!filterid)
    {
        mContext->alGenFilters(1, &filterid);
        if(alGetError() != AL_NO_ERROR)
            throw std::runtime_error("Failed to create Filter");
    }
    bool filterset = false;
    if(params.mGainHF < 1.0f && params.mGainLF < 1.0f)
    {
        mContext->alFilteri(filterid, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
        if(alGetError() == AL_NO_ERROR)
        {
            mContext->alFilterf(filterid, AL_BANDPASS_GAIN, std::min<ALfloat>(params.mGain, 1.0f));
            mContext->alFilterf(filterid, AL_BANDPASS_GAINHF, std::min<ALfloat>(params.mGainHF, 1.0f));
            mContext->alFilterf(filterid, AL_BANDPASS_GAINLF, std::min<ALfloat>(params.mGainLF, 1.0f));
            filterset = true;
        }
    }
    if(!filterset && !(params.mGainHF < 1.0f) && params.mGainLF < 1.0f)
    {
        mContext->alFilteri(filterid, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
        if(alGetError() == AL_NO_ERROR)
        {
            mContext->alFilterf(filterid, AL_HIGHPASS_GAIN, std::min<ALfloat>(params.mGain, 1.0f));
            mContext->alFilterf(filterid, AL_HIGHPASS_GAINLF, std::min<ALfloat>(params.mGainLF, 1.0f));
            filterset = true;
        }
    }
    if(!filterset)
    {
        mContext->alFilteri(filterid, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        if(alGetError() == AL_NO_ERROR)
        {
            mContext->alFilterf(filterid, AL_LOWPASS_GAIN, std::min<ALfloat>(params.mGain, 1.0f));
            mContext->alFilterf(filterid, AL_LOWPASS_GAINHF, std::min<ALfloat>(params.mGainHF, 1.0f));
            filterset = true;
        }
    }
}


void ALSource::setDirectFilter(const FilterParams &filter)
{
    if(!(filter.mGain >= 0.0f && filter.mGainHF >= 0.0f && filter.mGainLF >= 0.0f))
        throw std::runtime_error("Gain value out of range");
    CheckContext(mContext);

    setFilterParams(mDirectFilter, filter);
    if(mId)
        alSourcei(mId, AL_DIRECT_FILTER, mDirectFilter);
}

void ALSource::setSendFilter(ALuint send, const FilterParams &filter)
{
    if(!(filter.mGain >= 0.0f && filter.mGainHF >= 0.0f && filter.mGainLF >= 0.0f))
        throw std::runtime_error("Gain value out of range");
    CheckContext(mContext);

    SendPropMap::iterator siter = mEffectSlots.find(send);
    if(siter == mEffectSlots.end())
    {
        ALuint filterid = 0;

        setFilterParams(filterid, filter);
        if(!filterid) return;

        siter = mEffectSlots.insert(std::make_pair(send, SendProps(filterid))).first;
    }
    else
        setFilterParams(siter->second.mFilter, filter);

    if(mId)
    {
        ALuint slotid = (siter->second.mSlot ? siter->second.mSlot->getId() : 0);
        alSource3i(mId, AL_AUXILIARY_SEND_FILTER, slotid, send, siter->second.mFilter);
    }
}

void ALSource::setAuxiliarySend(AuxiliaryEffectSlot auxslot, ALuint send)
{
    ALAuxiliaryEffectSlot *slot = auxslot.getHandle();
    if(slot) CheckContext(slot->getContext());
    CheckContext(mContext);

    SendPropMap::iterator siter = mEffectSlots.find(send);
    if(siter == mEffectSlots.end())
    {
        if(!slot) return;
        slot->addSourceSend(Source(this), send);
        siter = mEffectSlots.insert(std::make_pair(send, SendProps(slot))).first;
    }
    else if(siter->second.mSlot != slot)
    {
        if(slot) slot->addSourceSend(Source(this), send);
        if(siter->second.mSlot)
            siter->second.mSlot->removeSourceSend(Source(this), send);
        siter->second.mSlot = slot;
    }

    if(mId)
    {
        ALuint slotid = (siter->second.mSlot ? siter->second.mSlot->getId() : 0);
        alSource3i(mId, AL_AUXILIARY_SEND_FILTER, slotid, send, siter->second.mFilter);
    }
}

void ALSource::setAuxiliarySendFilter(AuxiliaryEffectSlot auxslot, ALuint send, const FilterParams &filter)
{
    if(!(filter.mGain >= 0.0f && filter.mGainHF >= 0.0f && filter.mGainLF >= 0.0f))
        throw std::runtime_error("Gain value out of range");
    ALAuxiliaryEffectSlot *slot = auxslot.getHandle();
    if(slot) CheckContext(slot->getContext());
    CheckContext(mContext);

    SendPropMap::iterator siter = mEffectSlots.find(send);
    if(siter == mEffectSlots.end())
    {
        ALuint filterid = 0;

        setFilterParams(filterid, filter);
        if(!filterid && !slot)
            return;

        if(slot) slot->addSourceSend(Source(this), send);
        siter = mEffectSlots.insert(std::make_pair(send, SendProps(slot, filterid))).first;
    }
    else
    {
        if(siter->second.mSlot != slot)
        {
            if(slot) slot->addSourceSend(Source(this), send);
            if(siter->second.mSlot)
                siter->second.mSlot->removeSourceSend(Source(this), send);
            siter->second.mSlot = slot;
        }
        setFilterParams(siter->second.mFilter, filter);
    }

    if(mId)
    {
        ALuint slotid = (siter->second.mSlot ? siter->second.mSlot->getId() : 0);
        alSource3i(mId, AL_AUXILIARY_SEND_FILTER, slotid, send, siter->second.mFilter);
    }
}


void ALSource::release()
{
    CheckContext(mContext);

    if(mIsAsync.load(std::memory_order_acquire))
    {
        mContext->removeStream(this);
        mIsAsync.store(false, std::memory_order_release);
    }

    if(mId != 0)
    {
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        if(mContext->hasExtension(EXT_EFX))
        {
            alSourcei(mId, AL_DIRECT_FILTER, AL_FILTER_NULL);
            for(auto &i : mEffectSlots)
                alSource3i(mId, AL_AUXILIARY_SEND_FILTER, 0, i.first, AL_FILTER_NULL);
        }
        mContext->insertSourceId(mId);
        mId = 0;
    }

    mContext->freeSource(this);

    if(mDirectFilter)
        mContext->alDeleteFilters(1, &mDirectFilter);
    mDirectFilter = AL_FILTER_NULL;

    for(auto &i : mEffectSlots)
    {
        if(i.second.mSlot)
            i.second.mSlot->removeSourceSend(Source(this), i.first);
        if(i.second.mFilter)
            mContext->alDeleteFilters(1, &i.second.mFilter);
    }
    mEffectSlots.clear();

    if(mBuffer)
        mBuffer->removeSource(Source(this));
    mBuffer = 0;

    mStream.reset();

    resetProperties();
}


// Need to use these to avoid extraneous commas in macro parameter lists
using UInt64NSecPair = std::pair<uint64_t,std::chrono::nanoseconds>;
using SecondsPair = std::pair<Seconds,Seconds>;
using ALfloatPair = std::pair<ALfloat,ALfloat>;
using Vector3Pair = std::pair<Vector3,Vector3>;
using BoolTriple = std::tuple<bool,bool,bool>;

DECL_THUNK1(void, Source, play,, Buffer)
DECL_THUNK3(void, Source, play,, SharedPtr<Decoder>, ALuint, ALuint)
DECL_THUNK0(void, Source, stop,)
DECL_THUNK0(void, Source, pause,)
DECL_THUNK0(void, Source, resume,)
DECL_THUNK0(bool, Source, isPlaying, const)
DECL_THUNK0(bool, Source, isPaused, const)
DECL_THUNK1(void, Source, setPriority,, ALuint)
DECL_THUNK0(ALuint, Source, getPriority, const)
DECL_THUNK1(void, Source, setOffset,, uint64_t)
DECL_THUNK0(UInt64NSecPair, Source, getSampleOffsetLatency, const)
DECL_THUNK0(SecondsPair, Source, getSecOffsetLatency, const)
DECL_THUNK1(void, Source, setLooping,, bool)
DECL_THUNK0(bool, Source, getLooping, const)
DECL_THUNK1(void, Source, setPitch,, ALfloat)
DECL_THUNK0(ALfloat, Source, getPitch, const)
DECL_THUNK1(void, Source, setGain,, ALfloat)
DECL_THUNK0(ALfloat, Source, getGain, const)
DECL_THUNK2(void, Source, setGainRange,, ALfloat, ALfloat)
DECL_THUNK0(ALfloatPair, Source, getGainRange, const)
DECL_THUNK2(void, Source, setDistanceRange,, ALfloat, ALfloat)
DECL_THUNK0(ALfloatPair, Source, getDistanceRange, const)
DECL_THUNK3(void, Source, setPosition,, ALfloat, ALfloat, ALfloat)
DECL_THUNK1(void, Source, setPosition,, const ALfloat*)
DECL_THUNK0(Vector3, Source, getPosition, const)
DECL_THUNK3(void, Source, setVelocity,, ALfloat, ALfloat, ALfloat)
DECL_THUNK1(void, Source, setVelocity,, const ALfloat*)
DECL_THUNK0(Vector3, Source, getVelocity, const)
DECL_THUNK3(void, Source, setDirection,, ALfloat, ALfloat, ALfloat)
DECL_THUNK1(void, Source, setDirection,, const ALfloat*)
DECL_THUNK0(Vector3, Source, getDirection, const)
DECL_THUNK6(void, Source, setOrientation,, ALfloat, ALfloat, ALfloat, ALfloat, ALfloat, ALfloat)
DECL_THUNK2(void, Source, setOrientation,, const ALfloat*, const ALfloat*)
DECL_THUNK1(void, Source, setOrientation,, const ALfloat*)
DECL_THUNK0(Vector3Pair, Source, getOrientation, const)
DECL_THUNK2(void, Source, setConeAngles,, ALfloat, ALfloat)
DECL_THUNK0(ALfloatPair, Source, getConeAngles, const)
DECL_THUNK2(void, Source, setOuterConeGains,, ALfloat, ALfloat)
DECL_THUNK0(ALfloatPair, Source, getOuterConeGains, const)
DECL_THUNK2(void, Source, setRolloffFactors,, ALfloat, ALfloat)
DECL_THUNK0(ALfloatPair, Source, getRolloffFactors, const)
DECL_THUNK1(void, Source, setDopplerFactor,, ALfloat)
DECL_THUNK0(ALfloat, Source, getDopplerFactor, const)
DECL_THUNK1(void, Source, setRelative,, bool)
DECL_THUNK0(bool, Source, getRelative, const)
DECL_THUNK1(void, Source, setRadius,, ALfloat)
DECL_THUNK0(ALfloat, Source, getRadius, const)
DECL_THUNK2(void, Source, setStereoAngles,, ALfloat, ALfloat)
DECL_THUNK0(ALfloatPair, Source, getStereoAngles, const)
DECL_THUNK1(void, Source, set3DSpatialize,, Spatialize)
DECL_THUNK0(Spatialize, Source, get3DSpatialize, const)
DECL_THUNK1(void, Source, setResamplerIndex,, ALsizei)
DECL_THUNK0(ALsizei, Source, getResamplerIndex, const)
DECL_THUNK1(void, Source, setAirAbsorptionFactor,, ALfloat)
DECL_THUNK0(ALfloat, Source, getAirAbsorptionFactor, const)
DECL_THUNK3(void, Source, setGainAuto,, bool, bool, bool)
DECL_THUNK0(BoolTriple, Source, getGainAuto, const)
DECL_THUNK1(void, Source, setDirectFilter,, const FilterParams&)
DECL_THUNK2(void, Source, setSendFilter,, ALuint, const FilterParams&)
DECL_THUNK2(void, Source, setAuxiliarySend,, AuxiliaryEffectSlot, ALuint)
DECL_THUNK3(void, Source, setAuxiliarySendFilter,, AuxiliaryEffectSlot, ALuint, const FilterParams&)
DECL_THUNK0(void, Source, update,)
void Source::release()
{
    pImpl->release();
    pImpl = nullptr;
}

}
