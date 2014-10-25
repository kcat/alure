
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
    if(!isRemovable())
        throw std::runtime_error("Buffer is in use");

    alGetError();
    alDeleteBuffers(1, &mId);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Buffer failed to delete");
    mId = 0;

    delete this;
}

ALuint ALBuffer::getLength()
{
    CheckContextDevice(mDevice);

    ALint size=-1, bits=-1, chans=-1;
    alGetBufferi(mId, AL_SIZE, &size);
    alGetBufferi(mId, AL_BITS, &bits);
    alGetBufferi(mId, AL_CHANNELS, &chans);
    if(size < 0 || bits < 0 || chans < 0)
        throw std::runtime_error("Buffer format error");
    return size / chans * 8 / bits;
}

ALuint ALBuffer::getFrequency()
{
    CheckContextDevice(mDevice);

    ALint freq = -1;
    alGetBufferi(mId, AL_FREQUENCY, &freq);
    if(freq < 0)
        throw std::runtime_error("Buffer frequency error");
    return freq;
}

ALuint ALBuffer::getSize()
{
    CheckContextDevice(mDevice);

    ALint size = -1;
    alGetBufferi(mId, AL_SIZE, &size);
    if(size < 0)
        throw std::runtime_error("Buffer size error");
    return size;
}

bool ALBuffer::isRemovable() const
{
    return (mRefs.load() == 0);
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
        if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR8");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD8");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN8");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN8");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN8");
        }
        if(alIsExtensionPresent("AL_EXT_BFORMAT"))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_8");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_8");
        }
    }
    else if(type == SampleType_Int16)
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO16;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO16;
        if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR16");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD16");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN16");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN16");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN16");
        }
        if(alIsExtensionPresent("AL_EXT_BFORMAT"))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_16");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_16");
        }
    }
    else if(type == SampleType_Float32 && alIsExtensionPresent("AL_EXT_float32"))
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO_FLOAT32;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO_FLOAT32;
        if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR32");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD32");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN32");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN32");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN32");
        }
        if(alIsExtensionPresent("AL_EXT_BFORMAT"))
        {
            if(chans == SampleConfig_BFmt_WXY) RETURN_FMT("AL_FORMAT_BFORMAT2D_FLOAT32");
            if(chans == SampleConfig_BFmt_WXYZ) RETURN_FMT("AL_FORMAT_BFORMAT3D_FLOAT32");
        }
    }
    else if(type == SampleType_Mulaw && alIsExtensionPresent("AL_EXT_MULAW"))
    {
        if(chans == SampleConfig_Mono) return AL_FORMAT_MONO_MULAW;
        if(chans == SampleConfig_Stereo) return AL_FORMAT_STEREO_MULAW;
        if(alIsExtensionPresent("AL_EXT_MULAW_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) RETURN_FMT("AL_FORMAT_REAR_MULAW");
            if(chans == SampleConfig_Quad) RETURN_FMT("AL_FORMAT_QUAD_MULAW");
            if(chans == SampleConfig_X51) RETURN_FMT("AL_FORMAT_51CHN_MULAW");
            if(chans == SampleConfig_X61) RETURN_FMT("AL_FORMAT_61CHN_MULAW");
            if(chans == SampleConfig_X71) RETURN_FMT("AL_FORMAT_71CHN_MULAW");
        }
        if(alIsExtensionPresent("AL_EXT_MULAW_BFORMAT"))
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
