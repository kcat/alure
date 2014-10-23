
#include "mpg123-1.h"

#include <stdexcept>

namespace alure
{

Mpg123Decoder::Mpg123Decoder(mpg123_handle *mpg123, int chans, long srate)
  : mMpg123(mpg123), mChannels(chans), mSampleRate(srate)
{ }

Mpg123Decoder::~Mpg123Decoder()
{
    if(mMpg123)
    {
        mpg123_close(mMpg123);
        mpg123_delete(mMpg123);
        mMpg123 = 0;
    }
}


ALuint Mpg123Decoder::getFrequency()
{
    return mSampleRate;
}

SampleConfig Mpg123Decoder::getSampleConfig()
{
    if(mChannels == 1)
        return SampleConfig_Mono;
    if(mChannels == 2)
        return SampleConfig_Stereo;
    throw std::runtime_error("Unsupported sample configuration");
}

SampleType Mpg123Decoder::getSampleType()
{
    return SampleType_Int16;
}


ALuint Mpg123Decoder::getLength()
{
    off_t len = mpg123_length(mMpg123);
    return (ALuint)std::max<off_t>(len, 0);
}

ALuint Mpg123Decoder::getPosition()
{
    off_t pos = mpg123_tell(mMpg123);
    return (ALuint)std::max<off_t>(pos, 0);
}

bool Mpg123Decoder::seek(ALuint pos)
{
    off_t newpos = mpg123_seek(mMpg123, pos, SEEK_SET);
    if(newpos < 0) return false;
    if(newpos != pos)
        throw std::runtime_error("Unexpected seek offset");
    return true;
}

ALuint Mpg123Decoder::read(ALvoid *ptr, ALuint count)
{
    ALuint total = 0;
    while(total < count)
    {
        size_t got = 0;
        int ret = mpg123_read(mMpg123, reinterpret_cast<unsigned char*>(ptr), count*mChannels*2, &got);
        if((ret != MPG123_OK && ret != MPG123_DONE) || got == 0)
            break;
        total += got / mChannels / 2;
        if(ret == MPG123_DONE)
            break;
    }
    return total;
}


Decoder *Mpg123DecoderFactory::createDecoder(const std::string &name)
{
    static bool inited = false;
    if(!inited)
    {
        if(mpg123_init() != MPG123_OK)
            return 0;
        inited = true;
    }

    mpg123_handle *mpg123 = mpg123_new(0, 0);
    if(mpg123)
    {
        if(mpg123_open(mpg123, name.c_str()) == MPG123_OK)
        {
            int enc, channels;
            long srate;

            if(mpg123_getformat(mpg123, &srate, &channels, &enc) == MPG123_OK)
            {
                if((channels == 1 || channels == 2) && srate > 0 &&
                   mpg123_format_none(mpg123) == MPG123_OK &&
                   mpg123_format(mpg123, srate, channels, MPG123_ENC_SIGNED_16) == MPG123_OK)
                {
                    // All OK
                    return new Mpg123Decoder(mpg123, channels, srate);
                }
            }
            mpg123_close(mpg123);
        }
        mpg123_delete(mpg123);
        mpg123 = 0;
    }
    return 0;
}

}
