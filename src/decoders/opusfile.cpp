
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

class OpusFileDecoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    OggOpusFile *mOggFile;
    int mOggBitstream;

    ChannelConfig mChannelConfig;
    SampleType mSampleType;

    template<typename T>
    ALuint do_read(T *ptr, ALuint count) noexcept
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
    OpusFileDecoder(UniquePtr<std::istream> file, OggOpusFile *oggfile, ChannelConfig sconfig, SampleType stype) noexcept
      : mFile(std::move(file)), mOggFile(oggfile), mOggBitstream(0), mChannelConfig(sconfig), mSampleType(stype)
    { }
    ~OpusFileDecoder() override;

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

OpusFileDecoder::~OpusFileDecoder()
{
    op_free(mOggFile);
}


ALuint OpusFileDecoder::getFrequency() const noexcept
{
    // libopusfile always decodes to 48khz.
    return 48000;
}

ChannelConfig OpusFileDecoder::getChannelConfig() const noexcept
{
    return mChannelConfig;
}

SampleType OpusFileDecoder::getSampleType() const noexcept
{
    return mSampleType;
}


uint64_t OpusFileDecoder::getLength() const noexcept
{
    ogg_int64_t len = op_pcm_total(mOggFile, -1);
    return std::max<ogg_int64_t>(len, 0);
}

bool OpusFileDecoder::seek(uint64_t pos) noexcept
{
    return op_pcm_seek(mOggFile, pos) == 0;
}

std::pair<uint64_t,uint64_t> OpusFileDecoder::getLoopPoints() const noexcept
{
    return std::make_pair(0, std::numeric_limits<uint64_t>::max());
}

ALuint OpusFileDecoder::read(ALvoid *ptr, ALuint count) noexcept
{
    if(mSampleType == SampleType::Float32)
        return do_read<float>(reinterpret_cast<float*>(ptr), count);
    return do_read<ogg_int16_t>(reinterpret_cast<ogg_int16_t*>(ptr), count);
}


SharedPtr<Decoder> OpusFileDecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    static const OpusFileCallbacks streamIO = {
        read, seek, tell, nullptr
    };

    OggOpusFile *oggfile = op_open_callbacks(file.get(), &streamIO, nullptr, 0, nullptr);
    if(!oggfile) return nullptr;

    std::pair<uint64_t,uint64_t> loop_points = { 0, std::numeric_limits<uint64_t>::max() };
    if(const OpusTags *tags = op_tags(oggfile, -1))
    {
        for(int i = 0;i < tags->comments;i++)
        {
            auto seppos = StringView(
                tags->user_comments[i], tags->comment_lengths[i]
            ).find_first_of('=');
            if(seppos == StringView::npos) continue;

            StringView key(tags->user_comments[i], seppos);
            StringView val(tags->user_comments[i]+seppos+1, tags->comment_lengths[i]-(seppos+1));

            // RPG Maker seems to recognize LOOPSTART and LOOPLENGTH for loop
            // points in an Ogg file. ZDoom recognizes LOOP_START and LOOP_END.
            // We can recognize both.
            if(key == "LOOP_START" || key == "LOOPSTART")
            {
                auto pt = parse_timeval(val, 48000.0);
                if(pt.index() == 1) loop_points.first = std::get<1>(pt);
                continue;
            }

            if(key == "LOOP_END")
            {
                auto pt = parse_timeval(val, 48000.0);
                if(pt.index() == 1) loop_points.second = std::get<1>(pt);
                continue;
            }

            if(key == "LOOPLENGTH")
            {
                auto pt = parse_timeval(val, 48000.0);
                if(pt.index() == 1)
                    loop_points.second = loop_points.first + std::get<1>(pt);
                continue;
            }
        }
    }

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
