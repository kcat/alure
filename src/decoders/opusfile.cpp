
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


class OpusFileDecoder : public Decoder {
    SharedPtr<std::istream> mFile;

    OggOpusFile *mOggFile;
    int mOggBitstream;

    ChannelConfig mChannelConfig;

public:
    OpusFileDecoder(SharedPtr<std::istream> file, OggOpusFile *oggfile, ChannelConfig sconfig)
      : mFile(file), mOggFile(oggfile), mOggBitstream(0), mChannelConfig(sconfig)
    { }
    virtual ~OpusFileDecoder();

    virtual ALuint getFrequency() const final;
    virtual ChannelConfig getChannelConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
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
    return SampleType::Int16;
}


uint64_t OpusFileDecoder::getLength()
{
    ogg_int64_t len = op_pcm_total(mOggFile, -1);
    return std::max<ogg_int64_t>(len, 0);
}

uint64_t OpusFileDecoder::getPosition()
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
    ALuint total = 0;
    opus_int16 *samples = (opus_int16*)ptr;
    int num_chans = FramesToBytes(1, mChannelConfig, SampleType::UInt8);
    while(total < count)
    {
        if(num_chans != op_head(mOggFile, -1)->channel_count)
            break;
        int len = (count-total) * num_chans;

        long got = op_read(mOggFile, samples, len, &mOggBitstream);
        if(got <= 0) break;

        samples += got*num_chans;
        total += got;
    }

    // 1, 2, and 4 channel files decode into the same channel order as
    // OpenAL, however 6 (5.1), 7 (6.1), and 8 (7.1) channel files need to be
    // re-ordered.
    if(mChannelConfig == ChannelConfig::X51)
    {
        samples = (opus_int16*)ptr;
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
        samples = (opus_int16*)ptr;
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
        samples = (opus_int16*)ptr;
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


SharedPtr<Decoder> OpusFileDecoderFactory::createDecoder(SharedPtr<std::istream> file)
{
    const OpusFileCallbacks streamIO = {
        read, seek, tell, nullptr
    };

    OggOpusFile *oggfile = op_open_callbacks(file.get(), &streamIO, nullptr, 0, nullptr);
    if(!oggfile) return SharedPtr<Decoder>(nullptr);

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
        return SharedPtr<Decoder>(nullptr);
    }

    return SharedPtr<Decoder>(new OpusFileDecoder(file, oggfile, channels));
}

}
