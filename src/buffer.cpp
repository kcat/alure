
#include "buffer.h"

#include <stdexcept>

#include "al.h"
#include "alext.h"

#include "context.h"

namespace alure
{

void ALBuffer::cleanup()
{
    if(mRefs.load() != 0)
        throw std::runtime_error("Buffer is in use");

    alGetError();
    alDeleteBuffers(1, &mId);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Buffer failed to delete");
    mId = 0;

    delete this;
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
    }
    switch(type)
    {
        case SampleType_UInt8: size *= 1; break;
        case SampleType_Int16: size *= 2; break;
        case SampleType_Float32: size *= 4; break;
    }

    return size;
}

ALenum GetFormat(SampleConfig chans, SampleType type)
{
    ALenum format = AL_NONE;

    if(type == SampleType_UInt8)
    {
        if(chans == SampleConfig_Mono) format = AL_FORMAT_MONO8;
        else if(chans == SampleConfig_Stereo) format = AL_FORMAT_STEREO8;
        else if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) format = alGetEnumValue("AL_FORMAT_REAR8");
            else if(chans == SampleConfig_Quad) format = alGetEnumValue("AL_FORMAT_QUAD8");
            else if(chans == SampleConfig_X51) format = alGetEnumValue("AL_FORMAT_51CHN8");
            else if(chans == SampleConfig_X61) format = alGetEnumValue("AL_FORMAT_61CHN8");
            else if(chans == SampleConfig_X71) format = alGetEnumValue("AL_FORMAT_71CHN8");
        }
    }
    else if(type == SampleType_Int16)
    {
        if(chans == SampleConfig_Mono) format = AL_FORMAT_MONO16;
        else if(chans == SampleConfig_Stereo) format = AL_FORMAT_STEREO16;
        else if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) format = alGetEnumValue("AL_FORMAT_REAR16");
            else if(chans == SampleConfig_Quad) format = alGetEnumValue("AL_FORMAT_QUAD16");
            else if(chans == SampleConfig_X51) format = alGetEnumValue("AL_FORMAT_51CHN16");
            else if(chans == SampleConfig_X61) format = alGetEnumValue("AL_FORMAT_61CHN16");
            else if(chans == SampleConfig_X71) format = alGetEnumValue("AL_FORMAT_71CHN16");
        }
    }
    else if(type == SampleType_Float32 && alIsExtensionPresent("AL_EXT_float32"))
    {
        if(chans == SampleConfig_Mono) format = AL_FORMAT_MONO_FLOAT32;
        else if(chans == SampleConfig_Stereo) format = AL_FORMAT_STEREO_FLOAT32;
        else if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) format = alGetEnumValue("AL_FORMAT_REAR32");
            else if(chans == SampleConfig_Quad) format = alGetEnumValue("AL_FORMAT_QUAD32");
            else if(chans == SampleConfig_X51) format = alGetEnumValue("AL_FORMAT_51CHN32");
            else if(chans == SampleConfig_X61) format = alGetEnumValue("AL_FORMAT_61CHN32");
            else if(chans == SampleConfig_X71) format = alGetEnumValue("AL_FORMAT_71CHN32");
        }
    }

    if(format == AL_NONE || format == -1)
        throw std::runtime_error("Format not supported");
    return format;
}

}
