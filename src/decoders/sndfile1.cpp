
#include "sndfile1.h"

#include <stdexcept>

namespace alure
{

Decoder *SndFileDecoder::openFile(const char *name)
{
    SF_INFO sndinfo;
    SNDFILE *sndfile = sf_open(name, SFM_READ, &sndinfo);
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

void SndFileDecoder::getFormat(ALuint *srate, SampleConfig *chans, SampleType *type)
{
    *srate = mSndInfo.samplerate;
    if(mSndInfo.channels == 1)
        *chans = SampleConfig_Mono;
    else if(mSndInfo.channels == 2)
        *chans = SampleConfig_Stereo;
    else
        throw std::runtime_error("Unsupported sample configuration");
    *type = SampleType_Int16;
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

ALsizei SndFileDecoder::read(ALvoid *ptr, ALsizei count)
{
    return sf_readf_short(mSndFile, static_cast<short*>(ptr), count);
}

}
