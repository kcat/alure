
#include "mpg123.hpp"

#include <stdexcept>
#include <iostream>

#include "context.h"

#include "mpg123.h"

namespace {

ssize_t istream_read(void *user_data, void *ptr, size_t count)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    file->clear();

    file->read(reinterpret_cast<char*>(ptr), count);
    return file->gcount();
}

off_t istream_lseek(void *user_data, off_t offset, int whence)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    file->clear();

    if(!file->seekg(offset, std::ios::seekdir(whence)))
        return -1;
    return file->tellg();
}


struct Mpg123HandleDeleter {
    void operator()(mpg123_handle *ptr) const
    {
        mpg123_close(ptr);
        mpg123_delete(ptr);
    }
};
using Mpg123HandlePtr = alure::UniquePtr<mpg123_handle,Mpg123HandleDeleter>;

} // namespace

namespace alure {

class Mpg123Decoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    Mpg123HandlePtr mMpg123;
    ChannelConfig mChannels{ChannelConfig::Mono};
    SampleType mSampleType{SampleType::UInt8};
    int mSampleRate{0};

public:
    Mpg123Decoder(UniquePtr<std::istream> file, Mpg123HandlePtr mpg123, ChannelConfig chans,
                  SampleType stype, int srate) noexcept
      : mFile(std::move(file)), mMpg123(std::move(mpg123)), mChannels(chans), mSampleType(stype)
      , mSampleRate(srate)
    { }
    ~Mpg123Decoder() override { }

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

ALuint Mpg123Decoder::getFrequency() const noexcept { return mSampleRate; }
ChannelConfig Mpg123Decoder::getChannelConfig() const noexcept { return mChannels; }
SampleType Mpg123Decoder::getSampleType() const noexcept { return mSampleType; }

uint64_t Mpg123Decoder::getLength() const noexcept
{
    off_t len = mpg123_length(mMpg123.get());
    return (uint64_t)std::max<off_t>(len, 0);
}

bool Mpg123Decoder::seek(uint64_t pos) noexcept
{
    off_t newpos = mpg123_seek(mMpg123.get(), pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

std::pair<uint64_t,uint64_t> Mpg123Decoder::getLoopPoints() const noexcept
{
    return std::make_pair(0, std::numeric_limits<uint64_t>::max());
}

ALuint Mpg123Decoder::read(ALvoid *ptr, ALuint count) noexcept
{
    unsigned char *dst = reinterpret_cast<unsigned char*>(ptr);
    ALuint bytes = FramesToBytes(count, mChannels, mSampleType);
    ALuint total = 0;
    while(total < bytes)
    {
        size_t got = 0;
        int ret = mpg123_read(mMpg123.get(), dst+total, bytes-total, &got);
        if((ret != MPG123_OK && ret != MPG123_DONE) || got == 0)
            break;

        total += got;
        if(ret == MPG123_DONE)
            break;
    }
    return BytesToFrames(total, mChannels, mSampleType);
}


Mpg123DecoderFactory::Mpg123DecoderFactory() noexcept
  : mIsInited(false)
{
    mIsInited = (mpg123_init() == MPG123_OK);
}

Mpg123DecoderFactory::~Mpg123DecoderFactory()
{
    if(mIsInited) mpg123_exit();
    mIsInited = false;
}

SharedPtr<Decoder> Mpg123DecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    if(!mIsInited) return nullptr;

    Mpg123HandlePtr mpg123(mpg123_new(nullptr, nullptr));
    if(!mpg123) return nullptr;

    if(mpg123_replace_reader_handle(mpg123.get(), istream_read, istream_lseek, nullptr) != MPG123_OK ||
       mpg123_open_handle(mpg123.get(), file.get()) != MPG123_OK)
        return nullptr;

    int enc, chancount;
    long srate;
    if(mpg123_getformat(mpg123.get(), &srate, &chancount, &enc) != MPG123_OK)
        return nullptr;

    if(srate < 1 || srate >= std::numeric_limits<int>::max())
        return nullptr;

    ChannelConfig chans;
    if(chancount == 1)
        chans = ChannelConfig::Mono;
    else if(chancount == 2)
        chans = ChannelConfig::Stereo;
    else
        return nullptr;

    ContextImpl *ctx = ContextImpl::GetCurrent();
    SampleType stype;
    switch(enc)
    {
    case MPG123_ENC_UNSIGNED_8: stype = SampleType::UInt8; break;
    case MPG123_ENC_SIGNED_16: stype = SampleType::Int16; break;
    case MPG123_ENC_FLOAT_32:
        if(ctx->isSupported(chans, SampleType::Float32))
        {
            stype = SampleType::Float32;
            break;
        }
        /*fall-through*/
    default:
        // It's set to use a non-supported sample type, force signed 16-bit.
        if(mpg123_format_none(mpg123.get()) != MPG123_OK ||
           mpg123_format(mpg123.get(), srate, chancount, MPG123_ENC_SIGNED_16) != MPG123_OK)
            return nullptr;
        stype = SampleType::Int16;
    }

    return MakeShared<Mpg123Decoder>(std::move(file), std::move(mpg123), chans, stype, srate);
}

} // namespace alure
