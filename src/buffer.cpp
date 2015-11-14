
#include "config.h"

#include "buffer.h"

#include <stdexcept>
#include <sstream>

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

ALuint ALBuffer::getLength() const
{
    CheckContextDevice(mDevice);
    if(mLoadStatus != BufferLoad_Ready)
        throw std::runtime_error("Buffer not loaded");

    ALint size=-1, bits=-1, chans=-1;
    alGetBufferi(mId, AL_SIZE, &size);
    alGetBufferi(mId, AL_BITS, &bits);
    alGetBufferi(mId, AL_CHANNELS, &chans);
    if(size < 0 || bits < 0 || chans < 0)
        throw std::runtime_error("Buffer format error");
    return size / chans * 8 / bits;
}

ALuint ALBuffer::getFrequency() const
{
    CheckContextDevice(mDevice);
    return mFrequency;
}

SampleConfig ALBuffer::getSampleConfig() const
{
    CheckContextDevice(mDevice);
    return mSampleConfig;
}

SampleType ALBuffer::getSampleType() const
{
    CheckContextDevice(mDevice);
    return mSampleType;
}

ALuint ALBuffer::getSize() const
{
    CheckContextDevice(mDevice);
    if(mLoadStatus != BufferLoad_Ready)
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

    if(!ALContext::GetCurrent()->hasExtension(SOFT_loop_points))
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
    CheckContextDevice(mDevice);
    if(mLoadStatus != BufferLoad_Ready)
        throw std::runtime_error("Buffer not loaded");

    if(!ALContext::GetCurrent()->hasExtension(SOFT_loop_points))
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
    if(mLoadStatus == BufferLoad_Pending && mIsLoaded)
        mLoadStatus = BufferLoad_Ready;
    return mLoadStatus;
}


const char *GetSampleTypeName(SampleType type)
{
    switch(type)
    {
        case SampleType_UInt8: return "Unsigned 8-bit";
        case SampleType_Int16: return "Signed 16-bit";
        case SampleType_Float32: return "32-bit float";
        case SampleType_Mulaw: return "Mulaw";
    }
    throw std::runtime_error("Invalid type");
}

const char *GetSampleConfigName(SampleConfig cfg)
{
    switch(cfg)
    {
        case SampleConfig_Mono: return "Mono";
        case SampleConfig_Stereo: return "Stereo";
        case SampleConfig_Rear: return "Rear";
        case SampleConfig_Quad: return "Quadrophonic";
        case SampleConfig_X51: return "5.1 Surround";
        case SampleConfig_X61: return "6.1 Surround";
        case SampleConfig_X71: return "7.1 Surround";
        case SampleConfig_BFmt_WXY: return "B-Format 2D";
        case SampleConfig_BFmt_WXYZ: return "B-Format 3D";
    }
    throw std::runtime_error("Invalid config");
}


ALuint FramesToBytes(ALuint size, SampleConfig chans, SampleType type)
{
    switch(chans)
    {
        case SampleConfig_Mono: size *= 1; break;
        case SampleConfig_Stereo: size *= 2; break;
        case SampleConfig_Rear: size *= 2; break;
        case SampleConfig_Quad: size *= 4; break;
        case SampleConfig_X51: size *= 6; break;
        case SampleConfig_X61: size *= 7; break;
        case SampleConfig_X71: size *= 8; break;
        case SampleConfig_BFmt_WXY: size *= 3; break;
        case SampleConfig_BFmt_WXYZ: size *= 4; break;
    }
    switch(type)
    {
        case SampleType_UInt8: size *= 1; break;
        case SampleType_Int16: size *= 2; break;
        case SampleType_Float32: size *= 4; break;
        case SampleType_Mulaw: size *= 1; break;
    }

    return size;
}

ALenum GetFormat(SampleConfig chans, SampleType type)
{
#define RETURN_FMT(x) do {                \
    ALenum fmt = alGetEnumValue(x);       \
    if(fmt != AL_NONE && fmt != -1)       \
        return fmt;                       \
} while(0)
    if(type == SampleType_UInt8)
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO8;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO8;
        if(ALContext::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR8");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD8");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN8");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN8");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN8");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_8");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_8");
        }
    }
    else if(type == SampleType_Int16)
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO16;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO16;
        if(ALContext::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR16");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD16");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN16");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN16");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN16");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_16");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_16");
        }
    }
    else if(type == SampleType_Float32 && ALContext::GetCurrent()->hasExtension(EXT_FLOAT32))
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO_FLOAT32;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO_FLOAT32;
        if(ALContext::GetCurrent()->hasExtension(EXT_MCFORMATS))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR32");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD32");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN32");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN32");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN32");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_BFORMAT))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_FLOAT32");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_FLOAT32");
        }
    }
    else if(type == SampleType_Mulaw && ALContext::GetCurrent()->hasExtension(EXT_MULAW))
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO_MULAW;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO_MULAW;
        if(ALContext::GetCurrent()->hasExtension(EXT_MULAW_MCFORMATS))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR_MULAW");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD_MULAW");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN_MULAW");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN_MULAW");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN_MULAW");
        }
        if(ALContext::GetCurrent()->hasExtension(EXT_MULAW_BFORMAT))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_MULAW");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_MULAW");
        }
    }
#undef RETURN_FMT

    std::stringstream sstr;
    sstr<< "Format not supported ("<<GetSampleTypeName(type)<<", "<<GetSampleConfigName(chans)<<")";
    throw std::runtime_error(sstr.str());
}

}
