
#include "sndfile.hpp"

#include <stdexcept>
#include <iostream>

#include "sndfile.h"

namespace
{

constexpr alure::Array<int,1> CHANNELS_MONO      {{SF_CHANNEL_MAP_MONO}};
constexpr alure::Array<int,2> CHANNELS_STEREO    {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT}};
constexpr alure::Array<int,2> CHANNELS_REAR      {{SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT}};
constexpr alure::Array<int,4> CHANNELS_QUAD      {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT}};
constexpr alure::Array<int,6> CHANNELS_5DOT1     {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_SIDE_LEFT, SF_CHANNEL_MAP_SIDE_RIGHT}};
constexpr alure::Array<int,6> CHANNELS_5DOT1_REAR{{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT}};
constexpr alure::Array<int,7> CHANNELS_6DOT1     {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_REAR_CENTER, SF_CHANNEL_MAP_SIDE_LEFT, SF_CHANNEL_MAP_SIDE_RIGHT}};
constexpr alure::Array<int,8> CHANNELS_7DOT1     {{SF_CHANNEL_MAP_LEFT, SF_CHANNEL_MAP_RIGHT, SF_CHANNEL_MAP_CENTER, SF_CHANNEL_MAP_LFE, SF_CHANNEL_MAP_REAR_LEFT, SF_CHANNEL_MAP_REAR_RIGHT, SF_CHANNEL_MAP_SIDE_LEFT, SF_CHANNEL_MAP_SIDE_RIGHT}};
constexpr alure::Array<int,3> CHANNELS_BFORMAT2D {{SF_CHANNEL_MAP_AMBISONIC_B_W, SF_CHANNEL_MAP_AMBISONIC_B_X, SF_CHANNEL_MAP_AMBISONIC_B_Y}};
constexpr alure::Array<int,4> CHANNELS_BFORMAT3D {{SF_CHANNEL_MAP_AMBISONIC_B_W, SF_CHANNEL_MAP_AMBISONIC_B_X, SF_CHANNEL_MAP_AMBISONIC_B_Y, SF_CHANNEL_MAP_AMBISONIC_B_Z}};

}

namespace alure
{

static sf_count_t get_filelen(void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);
    sf_count_t len = -1;

    file->clear();
    std::streampos pos = file->tellg();
    if(pos != -1 && file->seekg(0, std::ios::end))
    {
        len = file->tellg();
        file->seekg(pos);
    }
    return len;
}

static sf_count_t seek(sf_count_t offset, int whence, void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);

    file->clear();
    if(!file->seekg(offset, std::ios::seekdir(whence)))
        return -1;
    return file->tellg();
}

static sf_count_t read(void *ptr, sf_count_t count, void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);

    file->clear();
    file->read(reinterpret_cast<char*>(ptr), count);
    return file->gcount();
}

static sf_count_t write(const void*, sf_count_t, void*)
{
    return -1;
}

static sf_count_t tell(void *user_data)
{
    std::istream *file = reinterpret_cast<std::istream*>(user_data);

    file->clear();
    return file->tellg();
}


class SndFileDecoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    SNDFILE *mSndFile;
    SF_INFO mSndInfo;

    ChannelConfig mChannelConfig;
    SampleType mSampleType;

public:
    SndFileDecoder(UniquePtr<std::istream> file, SNDFILE *sndfile, const SF_INFO sndinfo, ChannelConfig sconfig, SampleType stype) noexcept
      : mFile(std::move(file)), mSndFile(sndfile), mSndInfo(sndinfo)
      , mChannelConfig(sconfig), mSampleType(stype)
    { }
    ~SndFileDecoder() override;

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

SndFileDecoder::~SndFileDecoder()
{
    sf_close(mSndFile);
    mSndFile = 0;
}


ALuint SndFileDecoder::getFrequency() const noexcept
{
    return mSndInfo.samplerate;
}

ChannelConfig SndFileDecoder::getChannelConfig() const noexcept
{
    return mChannelConfig;
}

SampleType SndFileDecoder::getSampleType() const noexcept
{
    return mSampleType;
}


uint64_t SndFileDecoder::getLength() const noexcept
{
    return std::max<sf_count_t>(mSndInfo.frames, 0);
}

bool SndFileDecoder::seek(uint64_t pos) noexcept
{
    sf_count_t newpos = sf_seek(mSndFile, pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

std::pair<uint64_t,uint64_t> SndFileDecoder::getLoopPoints() const noexcept
{
    return std::make_pair(0, 0);
}

ALuint SndFileDecoder::read(ALvoid *ptr, ALuint count) noexcept
{
    sf_count_t got = 0;
    if(mSampleType == SampleType::Int16)
        got = sf_readf_short(mSndFile, static_cast<short*>(ptr), count);
    else if(mSampleType == SampleType::Float32)
        got = sf_readf_float(mSndFile, static_cast<float*>(ptr), count);
    return (ALuint)std::max<sf_count_t>(got, 0);
}


SharedPtr<Decoder> SndFileDecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    SF_VIRTUAL_IO vio = {
        get_filelen, seek,
        read, write, tell
    };
    SF_INFO sndinfo;
    SNDFILE *sndfile = sf_open_virtual(&vio, SFM_READ, &sndinfo, file.get());
    if(!sndfile) return nullptr;

    ChannelConfig sconfig;
    Vector<int> chanmap(sndinfo.channels);
    if(sf_command(sndfile, SFC_GET_CHANNEL_MAP_INFO, chanmap.data(), chanmap.size()*sizeof(int)) == SF_TRUE)
    {
        auto matches = [](const Vector<int> &first, ArrayView<int> second) -> bool
        {
            if(first.size() != second.size())
                return false;
            return std::equal(first.begin(), first.end(), second.begin());
        };

        if(matches(chanmap, CHANNELS_MONO))
            sconfig = ChannelConfig::Mono;
        else if(matches(chanmap, CHANNELS_STEREO))
            sconfig = ChannelConfig::Stereo;
        else if(matches(chanmap, CHANNELS_REAR))
            sconfig = ChannelConfig::Rear;
        else if(matches(chanmap, CHANNELS_QUAD))
            sconfig = ChannelConfig::Quad;
        else if(matches(chanmap, CHANNELS_5DOT1) || matches(chanmap, CHANNELS_5DOT1_REAR))
            sconfig = ChannelConfig::X51;
        else if(matches(chanmap, CHANNELS_6DOT1))
            sconfig = ChannelConfig::X61;
        else if(matches(chanmap, CHANNELS_7DOT1))
            sconfig = ChannelConfig::X71;
        else if(matches(chanmap, CHANNELS_BFORMAT2D))
            sconfig = ChannelConfig::BFormat2D;
        else if(matches(chanmap, CHANNELS_BFORMAT3D))
            sconfig = ChannelConfig::BFormat3D;
        else
        {
            sf_close(sndfile);
            return nullptr;
        }
    }
    else if(sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, nullptr, 0) == SF_AMBISONIC_B_FORMAT)
    {
        if(sndinfo.channels == 3)
            sconfig = ChannelConfig::BFormat2D;
        else if(sndinfo.channels == 4)
            sconfig = ChannelConfig::BFormat3D;
        else
        {
            sf_close(sndfile);
            return nullptr;
        }
    }
    else if(sndinfo.channels == 1)
        sconfig = ChannelConfig::Mono;
    else if(sndinfo.channels == 2)
        sconfig = ChannelConfig::Stereo;
    else
    {
        sf_close(sndfile);
        return nullptr;
    }

    SampleType stype = SampleType::Int16;
    switch(sndinfo.format&SF_FORMAT_SUBMASK)
    {
        case SF_FORMAT_FLOAT:
        case SF_FORMAT_DOUBLE:
        case SF_FORMAT_VORBIS:
            if(Context::GetCurrent().isSupported(sconfig, SampleType::Float32))
            {
                stype = SampleType::Float32;
                break;
            }
            /*fall-through*/
        default:
            stype = SampleType::Int16;
            break;
    }

    return MakeShared<SndFileDecoder>(std::move(file), sndfile, sndinfo, sconfig, stype);
}

}
