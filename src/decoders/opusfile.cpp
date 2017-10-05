
#include "opusfile.hpp"

#include <stdexcept>
#include <iostream>
#include <limits>

#include "buffer.h"

#include "opusfile.h"

namespace alure
{

static int read(void *user_data, unsigned char *ptr, int size)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    if(size < 0 || !stream->read(reinterpret_cast<char*>(ptr), size))
        return -1;
    return stream->gcount();
}

static int seek(void *user_data, opus_int64 offset, int whence)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    if(whence == SEEK_CUR)
        stream->seekg(offset, std::ios_base::cur);
    else if(whence == SEEK_SET)
        stream->seekg(offset, std::ios_base::beg);
    else if(whence == SEEK_END)
        stream->seekg(offset, std::ios_base::end);
    else
        return -1;

    return stream->good() ? 0 : -1;
}

static opus_int64 tell(void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();
    return stream->tellg();
}


template<typename T> struct OggTypeInfo { };
template<>
struct OggTypeInfo<ogg_int16_t>
{
    template<typename ...Args>
    static int read(Args&& ...args)
    { return op_read(std::forward<Args>(args)...); }
};
template<>
struct OggTypeInfo<float>
{
    template<typename ...Args>
    static int read(Args&& ...args)
    { return op_read_float(std::forward<Args>(args)...); }
};

class OpusFileDecoder : public Decoder {
    UniquePtr<std::istream> mFile;

    OggOpusFile *mOggFile;
    int mOggBitstream;

    ChannelConfig mChannelConfig;
    SampleType mSampleType;

    template<typename T>
    ALuint do_read(T *ptr, ALuint count)
    {
        ALuint total = 0;
        T *samples = ptr;
        int num_chans = FramesToBytes(1, mChannelConfig, SampleType::UInt8);
        while(total < count)
        {
            if(num_chans != op_head(mOggFile, -1)->channel_count)
                break;
            int len = (count-total) * num_chans;

            long got = OggTypeInfo<T>::read(mOggFile, samples, len, &mOggBitstream);
            if(got <= 0) break;

            samples += got*num_chans;
            total += got;
        }

        // 1, 2, and 4 channel files decode into the same channel order as
        // OpenAL, however 6 (5.1), 7 (6.1), and 8 (7.1) channel files need to be
        // re-ordered.
        if(mChannelConfig == ChannelConfig::X51)
        {
            samples = ptr;
            for(ALuint i = 0;i < total;++i)
            {
                // OpenAL : FL, FR, FC, LFE, RL, RR
                // Opus   : FL, FC, FR,  RL, RR, LFE
                std::swap(samples[i*6 + 1], samples[i*6 + 2]);
                std::swap(samples[i*6 + 3], samples[i*6 + 5]);
                std::swap(samples[i*6 + 4], samples[i*6 + 5]);
            }
        }
        else if(mChannelConfig == ChannelConfig::X61)
        {
            samples = ptr;
            for(ALuint i = 0;i < total;++i)
            {
                // OpenAL : FL, FR, FC, LFE, RC, SL, SR
                // Opus   : FL, FC, FR,  SL, SR, RC, LFE
                std::swap(samples[i*7 + 1], samples[i*7 + 2]);
                std::swap(samples[i*7 + 3], samples[i*7 + 6]);
                std::swap(samples[i*7 + 4], samples[i*7 + 5]);
                std::swap(samples[i*7 + 5], samples[i*7 + 6]);
            }
        }
        else if(mChannelConfig == ChannelConfig::X71)
        {
            samples = ptr;
            for(ALuint i = 0;i < total;++i)
            {
                // OpenAL : FL, FR, FC, LFE, RL, RR, SL, SR
                // Opus   : FL, FC, FR,  SL, SR, RL, RR, LFE
                std::swap(samples[i*8 + 1], samples[i*8 + 2]);
                std::swap(samples[i*8 + 3], samples[i*8 + 7]);
                std::swap(samples[i*8 + 4], samples[i*8 + 5]);
                std::swap(samples[i*8 + 5], samples[i*8 + 6]);
                std::swap(samples[i*8 + 6], samples[i*8 + 7]);
            }
        }

        return total;
    }

public:
    OpusFileDecoder(UniquePtr<std::istream> file, OggOpusFile *oggfile, ChannelConfig sconfig, SampleType stype)
      : mFile(std::move(file)), mOggFile(oggfile), mOggBitstream(0), mChannelConfig(sconfig), mSampleType(stype)
    { }
    ~OpusFileDecoder() override final;

    ALuint getFrequency() const override final;
    ChannelConfig getChannelConfig() const override final;
    SampleType getSampleType() const override final;

    uint64_t getLength() const override final;
    uint64_t getPosition() const override final;
    bool seek(uint64_t pos) override final;

    std::pair<uint64_t,uint64_t> getLoopPoints() const override final;

    ALuint read(ALvoid *ptr, ALuint count) override final;
};

OpusFileDecoder::~OpusFileDecoder()
{
    op_free(mOggFile);
}


ALuint OpusFileDecoder::getFrequency() const
{
    // libopusfile always decodes to 48khz.
    return 48000;
}

ChannelConfig OpusFileDecoder::getChannelConfig() const
{
    return mChannelConfig;
}

SampleType OpusFileDecoder::getSampleType() const
{
    return mSampleType;
}


uint64_t OpusFileDecoder::getLength() const
{
    ogg_int64_t len = op_pcm_total(mOggFile, -1);
    return std::max<ogg_int64_t>(len, 0);
}

uint64_t OpusFileDecoder::getPosition() const
{
    ogg_int64_t pos = op_pcm_tell(mOggFile);
    return std::max<ogg_int64_t>(pos, 0);
}

bool OpusFileDecoder::seek(uint64_t pos)
{
    return op_pcm_seek(mOggFile, pos) == 0;
}

std::pair<uint64_t,uint64_t> OpusFileDecoder::getLoopPoints() const
{
    return std::make_pair(0, std::numeric_limits<uint64_t>::max());
}

ALuint OpusFileDecoder::read(ALvoid *ptr, ALuint count)
{
    if(mSampleType == SampleType::Float32)
        return do_read<float>(reinterpret_cast<float*>(ptr), count);
    return do_read<ogg_int16_t>(reinterpret_cast<ogg_int16_t*>(ptr), count);
}


SharedPtr<Decoder> OpusFileDecoderFactory::createDecoder(UniquePtr<std::istream> &file)
{
    static const OpusFileCallbacks streamIO = {
        read, seek, tell, nullptr
    };

    OggOpusFile *oggfile = op_open_callbacks(file.get(), &streamIO, nullptr, 0, nullptr);
    if(!oggfile) return nullptr;

    int num_chans = op_head(oggfile, -1)->channel_count;
    ChannelConfig channels = ChannelConfig::Mono;
    if(num_chans == 1)
        channels = ChannelConfig::Mono;
    else if(num_chans == 2)
        channels = ChannelConfig::Stereo;
    else if(num_chans == 4)
        channels = ChannelConfig::Quad;
    else if(num_chans == 6)
        channels = ChannelConfig::X51;
    else if(num_chans == 7)
        channels = ChannelConfig::X61;
    else if(num_chans == 8)
        channels = ChannelConfig::X71;
    else
    {
        op_free(oggfile);
        return nullptr;
    }

    if(Context::GetCurrent().isSupported(channels, SampleType::Float32))
        return MakeShared<OpusFileDecoder>(std::move(file), oggfile, channels, SampleType::Float32);
    return MakeShared<OpusFileDecoder>(std::move(file), oggfile, channels, SampleType::Int16);
}

}
