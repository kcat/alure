
#include "sndfile1.h"

#include <stdexcept>

#include "sndfile.h"

namespace alure
{

class SndFileDecoder : public Decoder {
    SNDFILE *mSndFile;
    SF_INFO mSndInfo;

public:
    SndFileDecoder(SNDFILE *sndfile, const SF_INFO &info);
    virtual ~SndFileDecoder();

    virtual ALuint getFrequency() final;
    virtual SampleConfig getSampleConfig() final;
    virtual SampleType getSampleType() final;

    virtual ALuint getLength() final;
    virtual ALuint getPosition() final;
    virtual bool seek(ALuint pos) final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

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
    return true;
}

ALuint SndFileDecoder::read(ALvoid *ptr, ALuint count)
{
    sf_count_t got = sf_readf_short(mSndFile, static_cast<short*>(ptr), count);
    return (ALuint)std::max<sf_count_t>(got, 0);
}


Decoder *SndFileDecoderFactory::createDecoder(const std::string &name)
{
    SF_INFO sndinfo;
    SNDFILE *sndfile = sf_open(name.c_str(), SFM_READ, &sndinfo);
    if(!sndfile) return 0;
    return new SndFileDecoder(sndfile, sndinfo);
}

}
