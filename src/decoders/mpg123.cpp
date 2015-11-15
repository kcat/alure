
#include "mpg123.hpp"

#include <stdexcept>
#include <iostream>

#include "mpg123.h"

namespace alure
{

static ssize_t r_read(void *user_data, void *ptr, size_t count)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);

    file->clear();
    file->read(reinterpret_cast<char*>(ptr), count);
    return file->gcount();
}

static off_t r_lseek(void *user_data, off_t offset, int whence)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);

    file->clear();
    if(!file->seekg(offset, std::ios::seekdir(whence)))
        return -1;
    return file->tellg();
}


class Mpg123Decoder : public Decoder {
    SharedPtr<std::istream> mFile;

    mpg123_handle *mMpg123;
    int mChannels;
    long mSampleRate;

public:
    Mpg123Decoder(SharedPtr<std::istream> file, mpg123_handle *mpg123, int chans, long srate)
      : mFile(file), mMpg123(mpg123), mChannels(chans), mSampleRate(srate)
    { }
    virtual ~Mpg123Decoder();

    virtual bool isThreadSafe() const final { return true; }

    virtual ALuint getFrequency() const final;
    virtual SampleConfig getSampleConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

Mpg123Decoder::~Mpg123Decoder()
{
    mpg123_close(mMpg123);
    mpg123_delete(mMpg123);
    mMpg123 = 0;
}


ALuint Mpg123Decoder::getFrequency() const
{
    return mSampleRate;
}

SampleConfig Mpg123Decoder::getSampleConfig() const
{
    if(mChannels == 1)
        return SampleConfig_Mono;
    if(mChannels == 2)
        return SampleConfig_Stereo;
    throw std::runtime_error("Unsupported sample configuration");
}

SampleType Mpg123Decoder::getSampleType() const
{
    return SampleType_Int16;
}


uint64_t Mpg123Decoder::getLength()
{
    off_t len = mpg123_length(mMpg123);
    return (ALuint)std::max<off_t>(len, 0);
}

uint64_t Mpg123Decoder::getPosition()
{
    off_t pos = mpg123_tell(mMpg123);
    return (ALuint)std::max<off_t>(pos, 0);
}

bool Mpg123Decoder::seek(uint64_t pos)
{
    off_t newpos = mpg123_seek(mMpg123, pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

std::pair<uint64_t,uint64_t> Mpg123Decoder::getLoopPoints() const
{
    return std::make_pair(0, 0);
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


Mpg123DecoderFactory::Mpg123DecoderFactory()
  : mIsInited(false)
{
    if(!mIsInited)
    {
        if(mpg123_init() == MPG123_OK)
            mIsInited = true;
    }
}

Mpg123DecoderFactory::~Mpg123DecoderFactory()
{
    if(mIsInited)
        mpg123_exit();
    mIsInited = false;
}


SharedPtr<Decoder> Mpg123DecoderFactory::createDecoder(SharedPtr<std::istream> file)
{
    if(!mIsInited)
        return SharedPtr<Decoder>(nullptr);

    mpg123_handle *mpg123 = mpg123_new(0, 0);
    if(mpg123)
    {
        if(mpg123_replace_reader_handle(mpg123, r_read, r_lseek, 0) == MPG123_OK &&
           mpg123_open_handle(mpg123, file.get()) == MPG123_OK)
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
                    return SharedPtr<Decoder>(new Mpg123Decoder(file, mpg123, channels, srate));
                }
            }
            mpg123_close(mpg123);
        }
        mpg123_delete(mpg123);
    }

    return SharedPtr<Decoder>(nullptr);
}

}
