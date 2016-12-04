
#include "vorbisfile.hpp"

#include <stdexcept>
#include <iostream>

#include "vorbis/vorbisfile.h"

namespace alure
{

static int seek(void *user_data, ogg_int64_t offset, int whence)
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

    return stream->tellg();
}

static size_t read(void *ptr, size_t size, size_t nmemb, void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();

    stream->read(static_cast<char*>(ptr), nmemb*size);
    size_t ret = stream->gcount();
    return ret/size;
}

static long tell(void *user_data)
{
    std::istream *stream = static_cast<std::istream*>(user_data);
    stream->clear();
    return stream->tellg();
}

static int close(void*)
{
    return 0;
}


class VorbisFileDecoder : public Decoder {
    SharedPtr<std::istream> mFile;

    std::unique_ptr<OggVorbis_File> mOggFile;
    vorbis_info *mVorbisInfo;
    int mOggBitstream;

    ChannelConfig mChannelConfig;

public:
    VorbisFileDecoder(SharedPtr<std::istream> file, std::unique_ptr<OggVorbis_File>&& oggfile, vorbis_info *vorbisinfo, ChannelConfig sconfig)
      : mFile(file), mOggFile(std::move(oggfile)), mVorbisInfo(vorbisinfo), mOggBitstream(0), mChannelConfig(sconfig)
    { }
    virtual ~VorbisFileDecoder();

    virtual ALuint getFrequency() const final;
    virtual ChannelConfig getChannelConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

VorbisFileDecoder::~VorbisFileDecoder()
{
    ov_clear(mOggFile.get());
}


ALuint VorbisFileDecoder::getFrequency() const
{
    return mVorbisInfo->rate;
}

ChannelConfig VorbisFileDecoder::getChannelConfig() const
{
    return mChannelConfig;
}

SampleType VorbisFileDecoder::getSampleType() const
{
    return SampleType::Int16;
}


uint64_t VorbisFileDecoder::getLength()
{
    ogg_int64_t len = ov_pcm_total(mOggFile.get(), -1);
    return std::max<ogg_int64_t>(len, 0);
}

uint64_t VorbisFileDecoder::getPosition()
{
    ogg_int64_t pos = ov_pcm_tell(mOggFile.get());
    return std::max<ogg_int64_t>(pos, 0);
}

bool VorbisFileDecoder::seek(uint64_t pos)
{
    return ov_pcm_seek(mOggFile.get(), pos) == 0;
}

std::pair<uint64_t,uint64_t> VorbisFileDecoder::getLoopPoints() const
{
    return std::make_pair(0, 0);
}

ALuint VorbisFileDecoder::read(ALvoid *ptr, ALuint count)
{
    ALuint total = 0;
    ALshort *samples = (ALshort*)ptr;
    while(total < count)
    {
        int len = (count-total) * mVorbisInfo->channels * 2;
#ifdef __BIG_ENDIAN__
        long got = ov_read(mOggFile.get(), reinterpret_cast<char*>(samples), len, 1, 2, 1, &mOggBitstream);
#else
        long got = ov_read(mOggFile.get(), reinterpret_cast<char*>(samples), len, 0, 2, 1, &mOggBitstream);
#endif
        if(got <= 0) break;

        got /= 2;
        samples += got;
        got /= mVorbisInfo->channels;
        total += got;
    }

    // 1, 2, and 4 channel files decode into the same channel order as
    // OpenAL, however 6 (5.1), 7 (6.1), and 8 (7.1) channel files need to be
    // re-ordered.
    if(mChannelConfig == ChannelConfig::X51)
    {
        samples = (ALshort*)ptr;
        for(ALuint i = 0;i < total;++i)
        {
            // OpenAL : FL, FR, FC, LFE, RL, RR
            // Vorbis : FL, FC, FR,  RL, RR, LFE
            std::swap(samples[i*6 + 1], samples[i*6 + 2]);
            std::swap(samples[i*6 + 3], samples[i*6 + 5]);
            std::swap(samples[i*6 + 4], samples[i*6 + 5]);
        }
    }
    else if(mChannelConfig == ChannelConfig::X61)
    {
        samples = (ALshort*)ptr;
        for(ALuint i = 0;i < total;++i)
        {
            // OpenAL : FL, FR, FC, LFE, RC, SL, SR
            // Vorbis : FL, FC, FR,  SL, SR, RC, LFE
            std::swap(samples[i*7 + 1], samples[i*7 + 2]);
            std::swap(samples[i*7 + 3], samples[i*7 + 6]);
            std::swap(samples[i*7 + 4], samples[i*7 + 5]);
            std::swap(samples[i*7 + 5], samples[i*7 + 6]);
        }
    }
    else if(mChannelConfig == ChannelConfig::X71)
    {
        samples = (ALshort*)ptr;
        for(ALuint i = 0;i < total;++i)
        {
            // OpenAL : FL, FR, FC, LFE, RL, RR, SL, SR
            // Vorbis : FL, FC, FR,  SL, SR, RL, RR, LFE
            std::swap(samples[i*8 + 1], samples[i*8 + 2]);
            std::swap(samples[i*8 + 3], samples[i*8 + 7]);
            std::swap(samples[i*8 + 4], samples[i*8 + 5]);
            std::swap(samples[i*8 + 5], samples[i*8 + 6]);
            std::swap(samples[i*8 + 6], samples[i*8 + 7]);
        }
    }

    return total;
}


SharedPtr<Decoder> VorbisFileDecoderFactory::createDecoder(SharedPtr<std::istream> file)
{
    const ov_callbacks streamIO = {
        read, seek, close, tell
    };

    vorbis_info *vorbisinfo = nullptr;
    std::unique_ptr<OggVorbis_File> oggfile(new OggVorbis_File());
    if(ov_open_callbacks(file.get(), oggfile.get(), NULL, 0, streamIO) != 0)
        return nullptr;

    vorbisinfo = ov_info(oggfile.get(), -1);
    if(!vorbisinfo)
    {
        ov_clear(oggfile.get());
        return nullptr;
    }

    ChannelConfig channels = ChannelConfig::Mono;
    if(vorbisinfo->channels == 1)
        channels = ChannelConfig::Mono;
    else if(vorbisinfo->channels == 2)
        channels = ChannelConfig::Stereo;
    else if(vorbisinfo->channels == 4)
        channels = ChannelConfig::Quad;
    else if(vorbisinfo->channels == 6)
        channels = ChannelConfig::X51;
    else if(vorbisinfo->channels == 7)
        channels = ChannelConfig::X61;
    else if(vorbisinfo->channels == 8)
        channels = ChannelConfig::X71;
    else
    {
        ov_clear(oggfile.get());
        return nullptr;
    }

    return MakeShared<VorbisFileDecoder>(
        file, std::move(oggfile), vorbisinfo, channels
    );
}

}
