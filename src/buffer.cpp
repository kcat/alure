
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

void ALBuffer::cleanup()
{
    while(!mIsLoaded)
        std::this_thread::yield();
    if(isInUse())
        throw std::runtime_error("Buffer is in use");

    alGetError();
    alDeleteBuffers(1, &mId);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Buffer failed to delete");
    mId = 0;

    delete this;
}


void ALBuffer::load(ALuint frames, ALenum format, SharedPtr<Decoder> decoder, const String &name, ALContext *ctx)
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
        int silence = 0;
        if(mSampleType == SampleType::UInt8) silence = 0x80;
        else if(mSampleType == SampleType::Mulaw) silence = 0x7f;
        memset(data.data(), silence, data.size());
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

    mIsLoaded = true;
}


ALuint ALBuffer::getLength() const
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

ALuint ALBuffer::getSize() const
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

void ALBuffer::setLoopPoints(ALuint start, ALuint end)
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

std::pair<ALuint,ALuint> ALBuffer::getLoopPoints() const
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


BufferLoadStatus ALBuffer::getLoadStatus()
{
    /* NOTE: LoadStatus is separate from IsLoaded to force the app to receive
     * acknowledgement that the buffer is ready before using it. Otherwise, if
     * the app decides to simply use it after a short wait there's no guarantee
     * it'll be ready in a consistent manner. It may work some times and fail
     * others.
     */
    if(mLoadStatus == BufferLoadStatus::Pending && mIsLoaded)
        mLoadStatus = BufferLoadStatus::Ready;
    return mLoadStatus;
}


const char *GetSampleTypeName(SampleType type)
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

const char *GetChannelConfigName(ChannelConfig cfg)
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


ALuint FramesToBytes(ALuint size, ChannelConfig chans, SampleType type)
{
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
        if(ALContext::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR8");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD8");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN8");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN8");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN8");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_8");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_8");
        }
    }
    else if(type == SampleType::Int16)
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO16;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO16;
        if(ALContext::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR16");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD16");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN16");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN16");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN16");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_16");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_16");
        }
    }
    else if(type == SampleType::Float32 && ALContext::GetCurrent()->hasExtension(EXT_FLOAT32))
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO_FLOAT32;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO_FLOAT32;
        if(ALContext::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR32");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD32");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN32");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN32");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN32");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_FLOAT32");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_FLOAT32");
        }
    }
    else if(type == SampleType::Mulaw && ALContext::GetCurrent()->hasExtension(EXT_MULAW))
    {
        if(chans == ChannelConfig::Mono) return AL_FORMAT_MONO_MULAW;
        if(chans == ChannelConfig::Stereo) return AL_FORMAT_STEREO_MULAW;
        if(ALContext::GetCurrent()->hasExtension(EXT_MULAW_MCFORMATS))
        {
            if(chans == ChannelConfig::Rear) RETURN_FMT("AL_FORMAT_REAR_MULAW");
            if(chans == ChannelConfig::Quad) RETURN_FMT("AL_FORMAT_QUAD_MULAW");
            if(chans == ChannelConfig::X51) RETURN_FMT("AL_FORMAT_51CHN_MULAW");
            if(chans == ChannelConfig::X61) RETURN_FMT("AL_FORMAT_61CHN_MULAW");
            if(chans == ChannelConfig::X71) RETURN_FMT("AL_FORMAT_71CHN_MULAW");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_MULAW_BFORMAT))
        {
            if(chans == ChannelConfig::BFormat2D) RETURN_FMT("AL_FORMAT_BFORMAT2D_MULAW");
            if(chans == ChannelConfig::BFormat3D) RETURN_FMT("AL_FORMAT_BFORMAT3D_MULAW");
        }
    }
#undef RETURN_FMT

    std::stringstream sstr;
    sstr<< "Format not supported ("<<GetSampleTypeName(type)<<", "<<GetChannelConfigName(chans)<<")";
    throw std::runtime_error(sstr.str());
}

}
