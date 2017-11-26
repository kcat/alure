
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


class Mpg123Decoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    mpg123_handle *mMpg123{nullptr};
    int mChannels{0};
    long mSampleRate{0};

public:
    Mpg123Decoder(UniquePtr<std::istream> file, mpg123_handle *mpg123, int chans, long srate) noexcept
      : mFile(std::move(file)), mMpg123(mpg123), mChannels(chans), mSampleRate(srate)
    { }
    ~Mpg123Decoder() override;

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

Mpg123Decoder::~Mpg123Decoder()
{
    mpg123_close(mMpg123);
    mpg123_delete(mMpg123);
    mMpg123 = 0;
}


ALuint Mpg123Decoder::getFrequency() const noexcept
{
    return mSampleRate;
}

ChannelConfig Mpg123Decoder::getChannelConfig() const noexcept
{
    if(mChannels == 1)
        return ChannelConfig::Mono;
    /*if(mChannels == 2)*/
        return ChannelConfig::Stereo;
}

SampleType Mpg123Decoder::getSampleType() const noexcept
{
    return SampleType::Int16;
}


uint64_t Mpg123Decoder::getLength() const noexcept
{
    off_t len = mpg123_length(mMpg123);
    return (ALuint)std::max<off_t>(len, 0);
}

bool Mpg123Decoder::seek(uint64_t pos) noexcept
{
    off_t newpos = mpg123_seek(mMpg123, pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

std::pair<uint64_t,uint64_t> Mpg123Decoder::getLoopPoints() const noexcept
{
    return std::make_pair(0, 0);
}

ALuint Mpg123Decoder::read(ALvoid *ptr, ALuint count) noexcept
{
    unsigned char *dst = reinterpret_cast<unsigned char*>(ptr);
    ALuint bytes = count * mChannels * 2;
    ALuint total = 0;
    while(total < bytes)
    {
        size_t got = 0;
        int ret = mpg123_read(mMpg123, dst+total, bytes-total, &got);
        if((ret != MPG123_OK && ret != MPG123_DONE) || got == 0)
            break;

        total += got;
        if(ret == MPG123_DONE)
            break;
    }
    return total / mChannels / 2;
}


Mpg123DecoderFactory::Mpg123DecoderFactory() noexcept
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


SharedPtr<Decoder> Mpg123DecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    if(!mIsInited)
        return nullptr;

    mpg123_handle *mpg123 = mpg123_new(nullptr, nullptr);
    if(mpg123)
    {
        if(mpg123_replace_reader_handle(mpg123, r_read, r_lseek, nullptr) == MPG123_OK &&
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
                    return MakeShared<Mpg123Decoder>(std::move(file), mpg123, channels, srate);
                }
            }
            mpg123_close(mpg123);
        }
        mpg123_delete(mpg123);
    }

    return nullptr;
}

}
