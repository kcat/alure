
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

    uint64_t mSamplePos;
    std::pair<uint64_t,uint64_t> mLoopPts;
    bool mHasLooped;
    std::atomic<bool> mDone;

public:
    ALBufferStream(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint numupdates)
      : mDecoder(decoder), mUpdateLen(updatelen), mNumUpdates(numupdates),
        mFormat(AL_NONE), mFrequency(0), mFrameSize(0), mSilence(0),
        mCurrentIdx(0), mSamplePos(0), mLoopPts{0,0}, mHasLooped(false),
        mDone(false)
    { }
    ~ALBufferStream()
    {
        if(!mBufferIds.empty())
        {
            alDeleteBuffers(mBufferIds.size(), mBufferIds.data());
            mBufferIds.clear();
        }
    }

    uint64_t getLength() const { return mDecoder->getLength(); }
    uint64_t getPosition() const { return mSamplePos; }

    ALuint getNumUpdates() const { return mNumUpdates; }
    ALuint getUpdateLength() const { return mUpdateLen; }

    ALuint getFrequency() const { return mFrequency; }

    bool seek(uint64_t pos)
    {
        if(!mDecoder->seek(pos))
            return false;
        mSamplePos = pos;
        mHasLooped = false;
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
        alGenBuffers(mBufferIds.size(), mBufferIds.data());
    }

    int64_t getLoopStart() const { return mLoopPts.first; }
    int64_t getLoopEnd() const { return mLoopPts.second; }

    bool hasLooped() const { return mHasLooped; }
    bool hasMoreData() const { return !mDone.load(std::memory_order_acquire); }
    bool streamMoreData(ALuint srcid, bool loop)
    {
        if(mDone.load(std::memory_order_acquire))
            return false;

        ALuint frames;
        ALuint len = mUpdateLen;
        if(loop && mSamplePos <= mLoopPts.second)
            len = std::min<uint64_t>(len, mLoopPts.second - mSamplePos);
        else
            loop = false;

        frames = mDecoder->read(mData.data(), len);
        mSamplePos += frames;
        if(frames < mUpdateLen && loop && mSamplePos > 0)
        {
            if(mSamplePos < mLoopPts.second)
            {
                mLoopPts.second = mSamplePos;
                mLoopPts.first = std::min(mLoopPts.first, mLoopPts.second-1);
            }

            do {
                if(!mDecoder->seek(mLoopPts.first))
                    break;
                mSamplePos = mLoopPts.first;
                mHasLooped = true;

                len = std::min<uint64_t>(mUpdateLen-frames, mLoopPts.second-mLoopPts.first);
                ALuint got = mDecoder->read(&mData[frames*mFrameSize], len);
                if(got == 0) break;
                mSamplePos += got;
                frames += got;
            } while(frames < mUpdateLen);
        }
        if(frames < mUpdateLen)
        {
            mDone.store(true, std::memory_order_release);
            if(frames == 0) return false;
            mSamplePos += mUpdateLen - frames;
            std::fill(mData.begin() + frames*mFrameSize, mData.end(), mSilence);
        }

        alBufferData(mBufferIds[mCurrentIdx], mFormat, mData.data(), mData.size(), mFrequency);
        alSourceQueueBuffers(srcid, 1, &mBufferIds[mCurrentIdx]);
        mCurrentIdx = (mCurrentIdx+1) % mBufferIds.size();
        return true;
    }
};


SourceImpl::SourceImpl(ContextImpl *context)
  : mContext(context), mId(0), mBuffer(0), mGroup(nullptr), mIsAsync(false),
    mDirectFilter(AL_FILTER_NULL)
{
    resetProperties();
}

SourceImpl::~SourceImpl()
{
}


void SourceImpl::resetProperties()
{
    if(mGroup)
        mGroup->removeSource(Source(this));
    mGroup = nullptr;
    mGroupPitch = 1.0f;
    mGroupGain = 1.0f;

    mFadeGainTarget = 1.0f;
    mFadeGain = 1.0f;

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
            i.second.mSlot->removeSourceSend({Source(this), i.first});
        if(i.second.mFilter)
            mContext->alDeleteFilters(1, &i.second.mFilter);
    }
    mEffectSlots.clear();

    mPriority = 0;
}

void SourceImpl::applyProperties(bool looping, ALuint offset) const
{
    alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    alSourcei(mId, AL_SAMPLE_OFFSET, offset);
    alSourcef(mId, AL_PITCH, mPitch * mGroupPitch);
    alSourcef(mId, AL_GAIN, mGain * mGroupGain * mFadeGain);
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


void SourceImpl::setGroup(SourceGroupImpl *group)
{
    if(mGroup)
        mGroup->removeSource(Source(this));
    mGroup = group;
    mGroupPitch = mGroup->getAppliedPitch();
    mGroupGain = mGroup->getAppliedGain();
    if(mId)
    {
        alSourcef(mId, AL_PITCH, mPitch * mGroupPitch);
        alSourcef(mId, AL_GAIN, mGain * mGroupGain);
    }
}

void SourceImpl::unsetGroup()
{
    mGroup = nullptr;
    mGroupPitch = 1.0f;
    mGroupGain = 1.0f;
    if(mId)
    {
        alSourcef(mId, AL_PITCH, mPitch * mGroupPitch);
        alSourcef(mId, AL_GAIN, mGain * mGroupGain * mFadeGain);
    }
}

void SourceImpl::groupPropUpdate(ALfloat gain, ALfloat pitch)
{
    if(mId)
    {
        alSourcef(mId, AL_PITCH, mPitch * pitch);
        alSourcef(mId, AL_GAIN, mGain * gain * mFadeGain);
    }
    mGroupPitch = pitch;
    mGroupGain = gain;
}


void SourceImpl::play(Buffer buffer)
{
    BufferImpl *albuf = buffer.getHandle();
    if(!albuf) throw std::runtime_error("Buffer is not valid");
    CheckContext(mContext);
    CheckContext(albuf->getContext());

    if(mStream)
        mContext->removeStream(this);
    mIsAsync.store(false, std::memory_order_release);

    mFadeGainTarget = mFadeGain = 1.0f;
    mFadeTimeTarget = mLastFadeTime = std::chrono::steady_clock::now();

    if(mId == 0)
    {
        mId = mContext->getSourceId(mPriority);
        applyProperties(mLooping, (ALuint)std::min<uint64_t>(mOffset, std::numeric_limits<ALint>::max()));
    }
    else
    {
        mContext->removeFadingSource(this);
        mContext->removePlayingSource(this);
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
    mContext->removePendingSource(this);
    mContext->addPlayingSource(this, mId);
}

void SourceImpl::play(SharedPtr<Decoder> decoder, ALuint chunk_len, ALuint queue_size)
{
    if(chunk_len < 64)
        throw std::runtime_error("Update length out of range");
    if(queue_size < 2)
        throw std::runtime_error("Queue size out of range");
    CheckContext(mContext);

    auto stream = MakeUnique<ALBufferStream>(decoder, chunk_len, queue_size);
    stream->prepare();

    if(mStream)
        mContext->removeStream(this);
    mIsAsync.store(false, std::memory_order_release);

    mFadeGainTarget = mFadeGain = 1.0f;
    mFadeTimeTarget = mLastFadeTime = std::chrono::steady_clock::now();

    if(mId == 0)
    {
        mId = mContext->getSourceId(mPriority);
        applyProperties(false, 0);
    }
    else
    {
        mContext->removeFadingSource(this);
        mContext->removePlayingSource(this);
        alSourceRewind(mId);
        alSourcei(mId, AL_BUFFER, 0);
        alSourcei(mId, AL_LOOPING, AL_FALSE);
        alSourcei(mId, AL_SAMPLE_OFFSET, 0);
    }

    mStream.reset();
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
    mContext->removePendingSource(this);
    mContext->addPlayingSource(this);
}

void SourceImpl::play(SharedFuture<Buffer> future_buffer)
{
    if(!future_buffer.valid())
        throw std::runtime_error("Invalid future buffer");
    if(future_buffer.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready)
    {
        play(future_buffer.get());
        return;
    }

    CheckContext(mContext);

    mContext->removeFadingSource(this);
    mContext->removePlayingSource(this);
    makeStopped(true);

    mFadeGainTarget = mFadeGain = 1.0f;
    mFadeTimeTarget = mLastFadeTime = std::chrono::steady_clock::now();

    mContext->addPendingSource(this, std::move(future_buffer));
}


void SourceImpl::makeStopped(bool dolock)
{
    if(mStream)
    {
        if(dolock)
            mContext->removeStream(this);
        else
            mContext->removeStreamNoLock(this);
    }
    mIsAsync.store(false, std::memory_order_release);

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

    mStream.reset();
    if(mBuffer)
        mBuffer->removeSource(Source(this));
    mBuffer = 0;

    mPaused.store(false, std::memory_order_release);
}

void SourceImpl::stop()
{
    CheckContext(mContext);
    mContext->removePendingSource(this);
    mContext->removeFadingSource(this);
    mContext->removePlayingSource(this);
    makeStopped();
}


void SourceImpl::fadeOutToStop(ALfloat gain, std::chrono::milliseconds duration)
{
    if(!(gain < 1.0f && gain >= 0.0f))
        throw std::runtime_error("Fade gain target out of range");
    if(duration.count() <= 0)
        throw std::runtime_error("Fade duration out of range");
    CheckContext(mContext);

    mFadeGainTarget = std::max<ALfloat>(gain, 0.0001f);
    mLastFadeTime = std::chrono::steady_clock::now();
    mFadeTimeTarget = mLastFadeTime + duration;

    mContext->addFadingSource(this);
}


void SourceImpl::checkPaused()
{
    if(mPaused.load(std::memory_order_acquire) || mId == 0)
        return;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    // Streaming sources may be in a stopped or initial state if underrun
    mPaused.store(state == AL_PAUSED || (mStream && mStream->hasMoreData()),
                  std::memory_order_release);
}

void SourceImpl::pause()
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
        // Streaming sources may be in a stopped or initial state if underrun
        mPaused.store(state == AL_PAUSED || (mStream && mStream->hasMoreData()),
                      std::memory_order_release);
    }
}

void SourceImpl::resume()
{
    CheckContext(mContext);
    if(!mPaused.load(std::memory_order_acquire))
        return;

    if(mId != 0)
        alSourcePlay(mId);
    mPaused.store(false, std::memory_order_release);
}


bool SourceImpl::isPending() const
{
    CheckContext(mContext);
    return mContext->isPendingSource(this);
}

bool SourceImpl::isPlaying() const
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

bool SourceImpl::isPaused() const
{
    CheckContext(mContext);
    if(mId == 0) return false;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");

    return state == AL_PAUSED || mPaused.load(std::memory_order_acquire);
}


bool SourceImpl::checkPending(SharedFuture<Buffer> &future)
{
    if(future.wait_for(std::chrono::milliseconds::zero()) != std::future_status::ready)
        return true;

    BufferImpl *buffer = future.get().getHandle();
    if(Expect<false>(buffer->getContext() != mContext))
        return false;

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

    mBuffer = buffer;
    mBuffer->addSource(Source(this));

    alSourcei(mId, AL_BUFFER, mBuffer->getId());
    alSourcePlay(mId);
    mPaused.store(false, std::memory_order_release);
    mContext->addPlayingSource(this, mId);
    return false;
}

bool SourceImpl::fadeUpdate(std::chrono::steady_clock::time_point cur_fade_time)
{
    if((cur_fade_time - mFadeTimeTarget).count() >= 0)
    {
        mLastFadeTime = mFadeTimeTarget;
        mFadeGain = 1.0f;
        if(mFadeGainTarget >= 1.0f)
        {
            if(mId != 0)
                alSourcef(mId, AL_GAIN, mGain * mGroupGain);
            return false;
        }
        mContext->removePendingSource(this);
        mContext->removePlayingSource(this);
        makeStopped(true);
        return false;
    }

    float mult = std::pow(mFadeGainTarget/mFadeGain,
        float(1.0/Seconds(mFadeTimeTarget-mLastFadeTime).count())
    );

    std::chrono::steady_clock::duration duration = cur_fade_time - mLastFadeTime;
    mLastFadeTime = cur_fade_time;

    float gain = mFadeGain * std::pow(mult, (float)Seconds(duration).count());
    if(Expect<false>(gain == mFadeGain))
    {
        // Ensure the gain keeps moving toward its target, in case precision
        // loss results in no change with small steps.
        gain = std::nextafter(gain, mFadeGainTarget);
    }
    mFadeGain = gain;

    if(mId != 0)
        alSourcef(mId, AL_GAIN, mGain * mGroupGain * mFadeGain);
    return true;
}

bool SourceImpl::playUpdate(ALuint id)
{
    ALint state = -1;
    alGetSourcei(id, AL_SOURCE_STATE, &state);
    if(Expect<true>(state == AL_PLAYING || state == AL_PAUSED))
        return true;

    makeStopped();
    mContext->send(&MessageHandler::sourceStopped, Source(this));
    return false;
}

bool SourceImpl::playUpdate()
{
    if(Expect<true>(mIsAsync.load(std::memory_order_acquire)))
        return true;

    makeStopped();
    mContext->send(&MessageHandler::sourceStopped, Source(this));
    return false;
}


ALint SourceImpl::refillBufferStream()
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

bool SourceImpl::updateAsync()
{
    std::lock_guard<std::mutex> lock(mMutex);

    ALint queued = refillBufferStream();
    if(queued == 0)
    {
        mIsAsync.store(false, std::memory_order_release);
        return false;
    }

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(!mPaused.load(std::memory_order_acquire))
    {
        // Make sure the source is still playing if it's not paused.
        if(state != AL_PLAYING)
            alSourcePlay(mId);
    }
    else
    {
        // Rewind the source to an initial state if it underrun as it was
        // paused.
        if(state == AL_STOPPED)
            alSourceRewind(mId);
    }
    return true;
}


void SourceImpl::setPriority(ALuint priority)
{
    mPriority = priority;
}


void SourceImpl::setOffset(uint64_t offset)
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
        if(queued > 0 && !mPaused.load(std::memory_order_acquire))
            alSourcePlay(mId);
    }
}

std::pair<uint64_t,std::chrono::nanoseconds> SourceImpl::getSampleOffsetLatency() const
{
    std::pair<uint64_t,std::chrono::nanoseconds> ret{0, std::chrono::nanoseconds::zero()};
    CheckContext(mContext);
    if(mId == 0) return ret;

    if(mStream)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        ALint queued = 0, state = -1, srcpos = 0;

        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        if(mContext->hasExtension(SOFT_source_latency))
        {
            ALint64SOFT val[2];
            mContext->alGetSourcei64vSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
            srcpos = val[0]>>32;
            ret.second = std::chrono::nanoseconds(val[1]);
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

        ret.first = streampos;
        return ret;
    }

    ALint srcpos = 0;
    if(mContext->hasExtension(SOFT_source_latency))
    {
        ALint64SOFT val[2];
        mContext->alGetSourcei64vSOFT(mId, AL_SAMPLE_OFFSET_LATENCY_SOFT, val);
        srcpos = val[0]>>32;
        ret.second = std::chrono::nanoseconds(val[1]);
    }
    else
        alGetSourcei(mId, AL_SAMPLE_OFFSET, &srcpos);
    ret.first = srcpos;
    return ret;
}

std::pair<Seconds,Seconds> SourceImpl::getSecOffsetLatency() const
{
    std::pair<Seconds,Seconds> ret{Seconds::zero(), Seconds::zero()};
    CheckContext(mContext);
    if(mId == 0) return ret;

    if(mStream)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        ALint queued = 0, state = -1;
        ALdouble srcpos = 0;

        alGetSourcei(mId, AL_BUFFERS_QUEUED, &queued);
        if(mContext->hasExtension(SOFT_source_latency))
        {
            ALdouble val[2];
            mContext->alGetSourcedvSOFT(mId, AL_SEC_OFFSET_LATENCY_SOFT, val);
            srcpos = val[0];
            ret.second = Seconds(val[1]);
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

        ret.first = Seconds((streampos+frac) / mStream->getFrequency());
        return ret;
    }

    if(mContext->hasExtension(SOFT_source_latency))
    {
        ALdouble val[2];
        mContext->alGetSourcedvSOFT(mId, AL_SEC_OFFSET_LATENCY_SOFT, val);
        ret.first = Seconds(val[0]);
        ret.second = Seconds(val[1]);
    }
    else
    {
        ALfloat f;
        alGetSourcef(mId, AL_SEC_OFFSET, &f);
        ret.first = Seconds(f);
    }
    return ret;
}


void SourceImpl::setLooping(bool looping)
{
    CheckContext(mContext);

    if(mId && !mStream)
        alSourcei(mId, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    mLooping = looping;
}


void SourceImpl::setPitch(ALfloat pitch)
{
    if(!(pitch > 0.0f))
        throw std::runtime_error("Pitch out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_PITCH, pitch * mGroupPitch);
    mPitch = pitch;
}


void SourceImpl::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_GAIN, gain * mGroupGain * mFadeGain);
    mGain = gain;
}

void SourceImpl::setGainRange(ALfloat mingain, ALfloat maxgain)
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


void SourceImpl::setDistanceRange(ALfloat refdist, ALfloat maxdist)
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


void SourceImpl::set3DParameters(const Vector3 &position, const Vector3 &velocity, const Vector3 &direction)
{
    CheckContext(mContext);
    if(mId != 0)
    {
        Batcher batcher = mContext->getBatcher();
        alSourcefv(mId, AL_POSITION, position.getPtr());
        alSourcefv(mId, AL_VELOCITY, velocity.getPtr());
        alSourcefv(mId, AL_DIRECTION, direction.getPtr());
    }
    mPosition = position;
    mVelocity = velocity;
    mDirection = direction;
}

void SourceImpl::set3DParameters(const Vector3 &position, const Vector3 &velocity, std::pair<Vector3,Vector3> orientation)
{
    static_assert(sizeof(orientation) == sizeof(ALfloat[6]), "Invalid Vector3 pair size");
    CheckContext(mContext);
    if(mId != 0)
    {
        Batcher batcher = mContext->getBatcher();
        alSourcefv(mId, AL_POSITION, position.getPtr());
        alSourcefv(mId, AL_VELOCITY, velocity.getPtr());
        if(mContext->hasExtension(EXT_BFORMAT))
            alSourcefv(mId, AL_ORIENTATION, orientation.first.getPtr());
        alSourcefv(mId, AL_DIRECTION, orientation.first.getPtr());
    }
    mPosition = position;
    mVelocity = velocity;
    mDirection = mOrientation[0] = orientation.first;
    mOrientation[1] = orientation.second;
}


void SourceImpl::setPosition(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    if(mId != 0)
        alSource3f(mId, AL_POSITION, x, y, z);
    mPosition[0] = x;
    mPosition[1] = y;
    mPosition[2] = z;
}

void SourceImpl::setPosition(const ALfloat *pos)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcefv(mId, AL_POSITION, pos);
    mPosition[0] = pos[0];
    mPosition[1] = pos[1];
    mPosition[2] = pos[2];
}

void SourceImpl::setVelocity(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    if(mId != 0)
        alSource3f(mId, AL_VELOCITY, x, y, z);
    mVelocity[0] = x;
    mVelocity[1] = y;
    mVelocity[2] = z;
}

void SourceImpl::setVelocity(const ALfloat *vel)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcefv(mId, AL_VELOCITY, vel);
    mVelocity[0] = vel[0];
    mVelocity[1] = vel[1];
    mVelocity[2] = vel[2];
}

void SourceImpl::setDirection(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    if(mId != 0)
        alSource3f(mId, AL_DIRECTION, x, y, z);
    mDirection[0] = x;
    mDirection[1] = y;
    mDirection[2] = z;
}

void SourceImpl::setDirection(const ALfloat *dir)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcefv(mId, AL_DIRECTION, dir);
    mDirection[0] = dir[0];
    mDirection[1] = dir[1];
    mDirection[2] = dir[2];
}

void SourceImpl::setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2)
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

void SourceImpl::setOrientation(const ALfloat *at, const ALfloat *up)
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

void SourceImpl::setOrientation(const ALfloat *ori)
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


void SourceImpl::setConeAngles(ALfloat inner, ALfloat outer)
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

void SourceImpl::setOuterConeGains(ALfloat gain, ALfloat gainhf)
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


void SourceImpl::setRolloffFactors(ALfloat factor, ALfloat roomfactor)
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

void SourceImpl::setDopplerFactor(ALfloat factor)
{
    if(!(factor >= 0.0f && factor <= 1.0f))
        throw std::runtime_error("Doppler factor out of range");
    CheckContext(mContext);
    if(mId != 0)
        alSourcef(mId, AL_DOPPLER_FACTOR, factor);
    mDopplerFactor = factor;
}

void SourceImpl::setAirAbsorptionFactor(ALfloat factor)
{
    if(!(factor >= 0.0f && factor <= 10.0f))
        throw std::runtime_error("Absorption factor out of range");
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(EXT_EFX))
        alSourcef(mId, AL_AIR_ABSORPTION_FACTOR, factor);
    mAirAbsorptionFactor = factor;
}

void SourceImpl::setRadius(ALfloat radius)
{
    if(!(mRadius >= 0.0f))
        throw std::runtime_error("Radius out of range");
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(EXT_SOURCE_RADIUS))
        alSourcef(mId, AL_SOURCE_RADIUS, radius);
    mRadius = radius;
}

void SourceImpl::setStereoAngles(ALfloat leftAngle, ALfloat rightAngle)
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

void SourceImpl::set3DSpatialize(Spatialize spatialize)
{
    CheckContext(mContext);
    if(mId != 0 && mContext->hasExtension(SOFT_source_spatialize))
        alSourcei(mId, AL_SOURCE_SPATIALIZE_SOFT, (ALint)spatialize);
    mSpatialize = spatialize;
}

void SourceImpl::setResamplerIndex(ALsizei index)
{
    if(index < 0)
        throw std::runtime_error("Resampler index out of range");
    index = std::min<ALsizei>(index, mContext->getAvailableResamplers().size());
    if(mId != 0 && mContext->hasExtension(SOFT_source_resampler))
        alSourcei(mId, AL_SOURCE_RESAMPLER_SOFT, index);
    mResampler = index;
}

void SourceImpl::setRelative(bool relative)
{
    CheckContext(mContext);
    if(mId != 0)
        alSourcei(mId, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);
    mRelative = relative;
}

void SourceImpl::setGainAuto(bool directhf, bool send, bool sendhf)
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


void SourceImpl::setFilterParams(ALuint &filterid, const FilterParams &params)
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
            mContext->alFilterf(filterid, AL_BANDPASS_GAIN, std::min(params.mGain, 1.0f));
            mContext->alFilterf(filterid, AL_BANDPASS_GAINHF, std::min(params.mGainHF, 1.0f));
            mContext->alFilterf(filterid, AL_BANDPASS_GAINLF, std::min(params.mGainLF, 1.0f));
            filterset = true;
        }
    }
    if(!filterset && !(params.mGainHF < 1.0f) && params.mGainLF < 1.0f)
    {
        mContext->alFilteri(filterid, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
        if(alGetError() == AL_NO_ERROR)
        {
            mContext->alFilterf(filterid, AL_HIGHPASS_GAIN, std::min(params.mGain, 1.0f));
            mContext->alFilterf(filterid, AL_HIGHPASS_GAINLF, std::min(params.mGainLF, 1.0f));
            filterset = true;
        }
    }
    if(!filterset)
    {
        mContext->alFilteri(filterid, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
        if(alGetError() == AL_NO_ERROR)
        {
            mContext->alFilterf(filterid, AL_LOWPASS_GAIN, std::min(params.mGain, 1.0f));
            mContext->alFilterf(filterid, AL_LOWPASS_GAINHF, std::min(params.mGainHF, 1.0f));
            filterset = true;
        }
    }
}


void SourceImpl::setDirectFilter(const FilterParams &filter)
{
    if(!(filter.mGain >= 0.0f && filter.mGainHF >= 0.0f && filter.mGainLF >= 0.0f))
        throw std::runtime_error("Gain value out of range");
    CheckContext(mContext);

    setFilterParams(mDirectFilter, filter);
    if(mId)
        alSourcei(mId, AL_DIRECT_FILTER, mDirectFilter);
}

void SourceImpl::setSendFilter(ALuint send, const FilterParams &filter)
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

void SourceImpl::setAuxiliarySend(AuxiliaryEffectSlot auxslot, ALuint send)
{
    AuxiliaryEffectSlotImpl *slot = auxslot.getHandle();
    if(slot) CheckContext(slot->getContext());
    CheckContext(mContext);

    SendPropMap::iterator siter = mEffectSlots.find(send);
    if(siter == mEffectSlots.end())
    {
        if(!slot) return;
        slot->addSourceSend({Source(this), send});
        siter = mEffectSlots.insert(std::make_pair(send, SendProps(slot))).first;
    }
    else if(siter->second.mSlot != slot)
    {
        if(slot) slot->addSourceSend({Source(this), send});
        if(siter->second.mSlot)
            siter->second.mSlot->removeSourceSend({Source(this), send});
        siter->second.mSlot = slot;
    }

    if(mId)
    {
        ALuint slotid = (siter->second.mSlot ? siter->second.mSlot->getId() : 0);
        alSource3i(mId, AL_AUXILIARY_SEND_FILTER, slotid, send, siter->second.mFilter);
    }
}

void SourceImpl::setAuxiliarySendFilter(AuxiliaryEffectSlot auxslot, ALuint send, const FilterParams &filter)
{
    if(!(filter.mGain >= 0.0f && filter.mGainHF >= 0.0f && filter.mGainLF >= 0.0f))
        throw std::runtime_error("Gain value out of range");
    AuxiliaryEffectSlotImpl *slot = auxslot.getHandle();
    if(slot) CheckContext(slot->getContext());
    CheckContext(mContext);

    SendPropMap::iterator siter = mEffectSlots.find(send);
    if(siter == mEffectSlots.end())
    {
        ALuint filterid = 0;

        setFilterParams(filterid, filter);
        if(!filterid && !slot)
            return;

        if(slot) slot->addSourceSend({Source(this), send});
        siter = mEffectSlots.insert(std::make_pair(send, SendProps(slot, filterid))).first;
    }
    else
    {
        if(siter->second.mSlot != slot)
        {
            if(slot) slot->addSourceSend({Source(this), send});
            if(siter->second.mSlot)
                siter->second.mSlot->removeSourceSend({Source(this), send});
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


void SourceImpl::release()
{
    stop();

    resetProperties();
    mContext->freeSource(this);
}


// Need to use these to avoid extraneous commas in macro parameter lists
using UInt64NSecPair = std::pair<uint64_t,std::chrono::nanoseconds>;
using SecondsPair = std::pair<Seconds,Seconds>;
using ALfloatPair = std::pair<ALfloat,ALfloat>;
using Vector3Pair = std::pair<Vector3,Vector3>;
using BoolTriple = std::tuple<bool,bool,bool>;

DECL_THUNK1(void, Source, play,, Buffer)
DECL_THUNK3(void, Source, play,, SharedPtr<Decoder>, ALuint, ALuint)
DECL_THUNK1(void, Source, play,, SharedFuture<Buffer>)
DECL_THUNK0(void, Source, stop,)
DECL_THUNK2(void, Source, fadeOutToStop,, ALfloat, std::chrono::milliseconds)
DECL_THUNK0(void, Source, pause,)
DECL_THUNK0(void, Source, resume,)
DECL_THUNK0(bool, Source, isPending, const)
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
DECL_THUNK3(void, Source, set3DParameters,, const Vector3&, const Vector3&, const Vector3&)
DECL_THUNK3(void, Source, set3DParameters,, const Vector3&, const Vector3&, Vector3Pair)
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
void Source::release()
{
    pImpl->release();
    pImpl = nullptr;
}

}
