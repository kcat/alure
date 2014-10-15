
#include "sndfile1.h"

#include <stdexcept>

namespace alure
{

Decoder *SndFileDecoder::openFile(const std::string &name)
{
    SF_INFO sndinfo;
    SNDFILE *sndfile = sf_open(name.c_str(), SFM_READ, &sndinfo);
    if(!sndfile)
        throw std::runtime_error("Failed to open file");
    return new SndFileDecoder(sndfile, sndinfo);
}

SndFileDecoder::SndFileDecoder(SNDFILE *sndfile, const SF_INFO &info)
  : mSndFile(sndfile), mSndInfo(info)
{ }

SndFileDecoder::~SndFileDecoder()
{
    sf_close(mSndFile);
    mSndFile = 0;
}


ALuint SndFileDecoder::getFrequency()
{
    return mSndInfo.samplerate;
}

SampleConfig SndFileDecoder::getSampleConfig()
{
    if(mSndInfo.channels == 1)
        return SampleConfig_Mono;
    if(mSndInfo.channels == 2)
        return SampleConfig_Stereo;
    throw std::runtime_error("Unsupported sample configuration");
}

SampleType SndFileDecoder::getSampleType()
{
    return SampleType_Int16;
}


ALuint SndFileDecoder::getLength()
{
    return std::max<sf_count_t>(mSndInfo.frames, 0);
}

ALuint SndFileDecoder::getPosition()
{
    sf_count_t pos = sf_seek(mSndFile, 0, SEEK_CUR);
    return std::max<sf_count_t>(pos, 0);
}

bool SndFileDecoder::seek(ALuint pos)
{
    sf_count_t newpos = sf_seek(mSndFile, pos, SEEK_SET);
    if(newpos < 0) return false;
    if(newpos != pos)
        throw std::runtime_error("Unexpected seek offset");
    return true;
}

ALuint SndFileDecoder::read(ALvoid *ptr, ALuint count)
{
    sf_count_t got = sf_readf_short(mSndFile, static_cast<short*>(ptr), count);
    return (ALuint)std::max<sf_count_t>(got, 0);
}

}
