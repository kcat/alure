
#include "config.h"

#include "buffer.h"

#include <stdexcept>
#include <sstream>
#include <cstring>

#include "al.h"
#include "alext.h"

#include "context.h"

namespace alure
{

void BufferImpl::cleanup()
{
    while(!mIsLoaded.load(std::memory_order_acquire))
        std::this_thread::yield();
    if(isInUse())
        throw std::runtime_error("Buffer is in use");

    alGetError();
    alDeleteBuffers(1, &mId);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Buffer failed to delete");
    mId = 0;
}


void BufferImpl::load(ALuint frames, ALenum format, SharedPtr<Decoder> decoder, const String &name, ContextImpl *ctx)
{
    Vector<ALbyte> data(FramesToBytes(frames, mChannelConfig, mSampleType));

    ALuint got = decoder->read(data.data(), frames);
    if(got > 0)
    {
        frames = got;
        data.resize(FramesToBytes(frames, mChannelConfig, mSampleType));
    }
    else
    {
        ALbyte silence = 0;
        if(mSampleType == SampleType::UInt8) silence = 0x80;
        else if(mSampleType == SampleType::Mulaw) silence = 0x7f;
        std::fill(data.begin(), data.end(), silence);
    }

    std::pair<uint64_t,uint64_t> loop_pts = decoder->getLoopPoints();
    if(loop_pts.first >= loop_pts.second)
        loop_pts = std::make_pair(0, frames);
    else
    {
        loop_pts.second = std::min<uint64_t>(loop_pts.second, frames);
        loop_pts.first = std::min<uint64_t>(loop_pts.first, loop_pts.second-1);
    }

    ctx->send(&MessageHandler::bufferLoading,
        name, mChannelConfig, mSampleType, mFrequency, data
    );

    alBufferData(mId, format, data.data(), data.size(), mFrequency);
    if(ctx->hasExtension(SOFT_loop_points))
    {
        ALint pts[2]{(ALint)loop_pts.first, (ALint)loop_pts.second};
        alBufferiv(mId, AL_LOOP_POINTS_SOFT, pts);
    }

    mIsLoaded.store(true, std::memory_order_release);
}


ALuint BufferImpl::getLength() const
{
    CheckContext(mContext);
    if(mLoadStatus != BufferLoadStatus::Ready)
        throw std::runtime_error("Buffer not loaded");

    ALint size=-1, bits=-1, chans=-1;
    alGetBufferi(mId, AL_SIZE, &size);
    alGetBufferi(mId, AL_BITS, &bits);
    alGetBufferi(mId, AL_CHANNELS, &chans);
    if(size < 0 || bits < 0 || chans < 0)
        throw std::runtime_error("Buffer format error");
    return size / chans * 8 / bits;
}

ALuint BufferImpl::getSize() const
{
    CheckContext(mContext);
    if(mLoadStatus != BufferLoadStatus::Ready)
        throw std::runtime_error("Buffer not loaded");

    ALint size = -1;
    alGetBufferi(mId, AL_SIZE, &size);
    if(size < 0)
        throw std::runtime_error("Buffer size error");
    return size;
}

void BufferImpl::setLoopPoints(ALuint start, ALuint end)
{
    ALuint length = getLength();

    if(isInUse())
        throw std::runtime_error("Buffer is in use");

    if(!mContext->hasExtension(SOFT_loop_points))
    {
        if(start != 0 || end != length)
            throw std::runtime_error("Loop points not supported");
        return;
    }

    if(start >= end || end > length)
        throw std::runtime_error("Loop points out of range");

    ALint pts[2]{(ALint)start, (ALint)end};
    alGetError();
    alBufferiv(mId, AL_LOOP_POINTS_SOFT, pts);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Failed to set loop points");
}

std::pair<ALuint,ALuint> BufferImpl::getLoopPoints() const
{
    CheckContext(mContext);
    if(mLoadStatus != BufferLoadStatus::Ready)
        throw std::runtime_error("Buffer not loaded");

    if(!mContext->hasExtension(SOFT_loop_points))
        return std::make_pair(0, getLength());

    ALint pts[2]{-1,-1};
    alGetBufferiv(mId, AL_LOOP_POINTS_SOFT, pts);
    if(pts[0] == -1 || pts[1] == -1)
        throw std::runtime_error("Failed to get loop points");

    return std::make_pair(pts[0], pts[1]);
}


BufferLoadStatus BufferImpl::getLoadStatus()
{
    /* NOTE: LoadStatus is separate from IsLoaded to force the app to receive
     * acknowledgement that the buffer is ready before using it. Otherwise, if
     * the app decides to simply use it after a short wait there's no guarantee
     * it'll be ready in a consistent manner. It may work some times and fail
     * others.
     */
    if(mLoadStatus == BufferLoadStatus::Pending && mIsLoaded.load(std::memory_order_acquire))
        mLoadStatus = BufferLoadStatus::Ready;
    return mLoadStatus;
}

using ALuintPair = std::pair<ALuint,ALuint>;
DECL_THUNK0(ALuint, Buffer, getLength, const)
DECL_THUNK0(ALuint, Buffer, getFrequency, const)
DECL_THUNK0(ChannelConfig, Buffer, getChannelConfig, const)
DECL_THUNK0(SampleType, Buffer, getSampleType, const)
DECL_THUNK0(ALuint, Buffer, getSize, const)
DECL_THUNK2(void, Buffer, setLoopPoints,, ALuint, ALuint)
DECL_THUNK0(ALuintPair, Buffer, getLoopPoints, const)
DECL_THUNK0(Vector<Source>, Buffer, getSources, const)
DECL_THUNK0(BufferLoadStatus, Buffer, getLoadStatus,)
DECL_THUNK0(const String&, Buffer, getName, const)
DECL_THUNK0(bool, Buffer, isInUse, const)


ALURE_API const char *GetSampleTypeName(SampleType type)
{
    switch(type)
    {
        case SampleType::UInt8: return "Unsigned 8-bit";
        case SampleType::Int16: return "Signed 16-bit";
        case SampleType::Float32: return "32-bit float";
        case SampleType::Mulaw: return "Mulaw";
    }
    throw std::runtime_error("Invalid type");
}

ALURE_API const char *GetChannelConfigName(ChannelConfig cfg)
{
    switch(cfg)
    {
        case ChannelConfig::Mono: return "Mono";
        case ChannelConfig::Stereo: return "Stereo";
        case ChannelConfig::Rear: return "Rear";
        case ChannelConfig::Quad: return "Quadrophonic";
        case ChannelConfig::X51: return "5.1 Surround";
        case ChannelConfig::X61: return "6.1 Surround";
        case ChannelConfig::X71: return "7.1 Surround";
        case ChannelConfig::BFormat2D: return "B-Format 2D";
        case ChannelConfig::BFormat3D: return "B-Format 3D";
    }
    throw std::runtime_error("Invalid config");
}


ALURE_API ALuint FramesToBytes(ALuint frames, ChannelConfig chans, SampleType type)
{
    ALuint size = frames;
    switch(chans)
    {
        case ChannelConfig::Mono: size *= 1; break;
        case ChannelConfig::Stereo: size *= 2; break;
        case ChannelConfig::Rear: size *= 2; break;
        case ChannelConfig::Quad: size *= 4; break;
        case ChannelConfig::X51: size *= 6; break;
        case ChannelConfig::X61: size *= 7; break;
        case ChannelConfig::X71: size *= 8; break;
        case ChannelConfig::BFormat2D: size *= 3; break;
        case ChannelConfig::BFormat3D: size *= 4; break;
    }
    switch(type)
    {
        case SampleType::UInt8: size *= 1; break;
        case SampleType::Int16: size *= 2; break;
        case SampleType::Float32: size *= 4; break;
        case SampleType::Mulaw: size *= 1; break;
    }

    return size;
}

ALURE_API ALuint BytesToFrames(ALuint bytes, ChannelConfig chans, SampleType type)
{
    return bytes / FramesToBytes(1, chans, type);
}


ALenum GetFormat(ChannelConfig chans, SampleType type)
{
#define RETURN_FMT(x) do {                \
    ALenum fmt = alGetEnumValue(x);       \
    if(fmt != AL_NONE && fmt != -1)       \
        return fmt;                       \
} while(0)
    if(type == SampleType::UInt8)
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO8;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO8;
        if(ContextImpl::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR8");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD8");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN8");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN8");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN8");
        }
        if(ContextImpl::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_8");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_8");
        }
    }
    else if(type == SampleType::Int16)
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO16;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO16;
        if(ContextImpl::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR16");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD16");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN16");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN16");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN16");
        }
        if(ContextImpl::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_16");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_16");
        }
    }
    else if(type == SampleType::Float32 && ContextImpl::GetCurrent()->hasExtension(EXT_FLOAT32))
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO_FLOAT32;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO_FLOAT32;
        if(ContextImpl::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR32");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD32");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN32");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN32");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN32");
        }
        if(ContextImpl::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_FLOAT32");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_FLOAT32");
        }
    }
    else if(type == SampleType::Mulaw && ContextImpl::GetCurrent()->hasExtension(EXT_MULAW))
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO_MULAW;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO_MULAW;
        if(ContextImpl::GetCurrent()->hasExtension(EXT_MULAW_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR_MULAW");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD_MULAW");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN_MULAW");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN_MULAW");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN_MULAW");
        }
        if(ContextImpl::GetCurrent()->hasExtension(EXT_MULAW_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_MULAW");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_MULAW");
        }
    }
#undef RETURN_FMT

    return AL_NONE;
}

}
