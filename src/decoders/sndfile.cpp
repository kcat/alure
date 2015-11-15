
#include "sndfile.hpp"

#include <stdexcept>
#include <iostream>

#include "sndfile.h"

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


class SndFileDecoder : public Decoder {
    SharedPtr<std::istream> mFile;

    SNDFILE *mSndFile;
    SF_INFO mSndInfo;

    SampleConfig mSampleConfig;
    SampleType mSampleType;

public:
    SndFileDecoder(SharedPtr<std::istream> file, SNDFILE *sndfile, const SF_INFO sndinfo, SampleConfig sconfig, SampleType stype)
      : mFile(file), mSndFile(sndfile), mSndInfo(sndinfo), mSampleConfig(sconfig), mSampleType(stype)
    { }
    virtual ~SndFileDecoder();

    virtual ALuint getFrequency() const final;
    virtual SampleConfig getSampleConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

SndFileDecoder::~SndFileDecoder()
{
    sf_close(mSndFile);
    mSndFile = 0;
}


ALuint SndFileDecoder::getFrequency() const
{
    return mSndInfo.samplerate;
}

SampleConfig SndFileDecoder::getSampleConfig() const
{
    return mSampleConfig;
}

SampleType SndFileDecoder::getSampleType() const
{
    return mSampleType;
}


uint64_t SndFileDecoder::getLength()
{
    return std::max<sf_count_t>(mSndInfo.frames, 0);
}

uint64_t SndFileDecoder::getPosition()
{
    sf_count_t pos = sf_seek(mSndFile, 0, SEEK_CUR);
    return std::max<sf_count_t>(pos, 0);
}

bool SndFileDecoder::seek(uint64_t pos)
{
    sf_count_t newpos = sf_seek(mSndFile, pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

std::pair<uint64_t,uint64_t> SndFileDecoder::getLoopPoints() const
{
    return std::make_pair(0, 0);
}

ALuint SndFileDecoder::read(ALvoid *ptr, ALuint count)
{
    sf_count_t got = 0;
    if(mSampleType == SampleType_Int16)
        got = sf_readf_short(mSndFile, static_cast<short*>(ptr), count);
    else if(mSampleType == SampleType_Float32)
        got = sf_readf_float(mSndFile, static_cast<float*>(ptr), count);
    return (ALuint)std::max<sf_count_t>(got, 0);
}


SharedPtr<Decoder> SndFileDecoderFactory::createDecoder(SharedPtr<std::istream> file)
{
    SF_VIRTUAL_IO vio = {
        get_filelen, seek,
        read, write, tell
    };
    SF_INFO sndinfo;
    SNDFILE *sndfile = sf_open_virtual(&vio, SFM_READ, &sndinfo, file.get());

    SampleConfig sconfig;
    if(sndfile)
    {
        std::vector<int> chanmap(sndinfo.channels);
        if(sf_command(sndfile, SFC_GET_CHANNEL_MAP_INFO, &chanmap[0], chanmap.size()*sizeof(int)) == SF_TRUE)
        {
            if(sndinfo.channels == 1 && chanmap[0] == SF_CHANNEL_MAP_MONO)
                sconfig = SampleConfig_Mono;
            else if(sndinfo.channels == 2 &&
                    chanmap[0] == SF_CHANNEL_MAP_LEFT && chanmap[1] == SF_CHANNEL_MAP_RIGHT)
                sconfig = SampleConfig_Stereo;
            else if(sndinfo.channels == 2 &&
                    chanmap[0] == SF_CHANNEL_MAP_REAR_LEFT && chanmap[1] == SF_CHANNEL_MAP_REAR_RIGHT)
                sconfig = SampleConfig_Rear;
            else if(sndinfo.channels == 4 &&
                    chanmap[0] == SF_CHANNEL_MAP_LEFT && chanmap[1] == SF_CHANNEL_MAP_RIGHT &&
                    chanmap[2] == SF_CHANNEL_MAP_REAR_LEFT && chanmap[3] == SF_CHANNEL_MAP_REAR_RIGHT)
                sconfig = SampleConfig_Quad;
            else if(sndinfo.channels == 6 &&
                    chanmap[0] == SF_CHANNEL_MAP_LEFT && chanmap[1] == SF_CHANNEL_MAP_RIGHT &&
                    chanmap[2] == SF_CHANNEL_MAP_CENTER && chanmap[3] == SF_CHANNEL_MAP_LFE &&
                    chanmap[4] == SF_CHANNEL_MAP_REAR_LEFT && chanmap[5] == SF_CHANNEL_MAP_REAR_RIGHT)
                sconfig = SampleConfig_X51;
            else if(sndinfo.channels == 6 &&
                    chanmap[0] == SF_CHANNEL_MAP_LEFT && chanmap[1] == SF_CHANNEL_MAP_RIGHT &&
                    chanmap[2] == SF_CHANNEL_MAP_CENTER && chanmap[3] == SF_CHANNEL_MAP_LFE &&
                    chanmap[4] == SF_CHANNEL_MAP_SIDE_LEFT && chanmap[5] == SF_CHANNEL_MAP_SIDE_RIGHT)
                sconfig = SampleConfig_X51;
            else if(sndinfo.channels == 7 &&
                    chanmap[0] == SF_CHANNEL_MAP_LEFT && chanmap[1] == SF_CHANNEL_MAP_RIGHT &&
                    chanmap[2] == SF_CHANNEL_MAP_CENTER && chanmap[3] == SF_CHANNEL_MAP_LFE &&
                    chanmap[4] == SF_CHANNEL_MAP_REAR_CENTER && chanmap[5] == SF_CHANNEL_MAP_SIDE_LEFT &&
                    chanmap[6] == SF_CHANNEL_MAP_SIDE_RIGHT)
                sconfig = SampleConfig_X61;
            else if(sndinfo.channels == 8 &&
                    chanmap[0] == SF_CHANNEL_MAP_LEFT && chanmap[1] == SF_CHANNEL_MAP_RIGHT &&
                    chanmap[2] == SF_CHANNEL_MAP_CENTER && chanmap[3] == SF_CHANNEL_MAP_LFE &&
                    chanmap[4] == SF_CHANNEL_MAP_REAR_LEFT && chanmap[5] == SF_CHANNEL_MAP_REAR_RIGHT &&
                    chanmap[6] == SF_CHANNEL_MAP_SIDE_LEFT && chanmap[7] == SF_CHANNEL_MAP_SIDE_RIGHT)
                sconfig = SampleConfig_X51;
            else if(sndinfo.channels == 3 &&
                    chanmap[0] == SF_CHANNEL_MAP_AMBISONIC_B_W && chanmap[1] == SF_CHANNEL_MAP_AMBISONIC_B_X &&
                    chanmap[2] == SF_CHANNEL_MAP_AMBISONIC_B_Y)
                sconfig = SampleConfig_BFmt_WXY;
            else if(sndinfo.channels == 4 &&
                    chanmap[0] == SF_CHANNEL_MAP_AMBISONIC_B_W && chanmap[1] == SF_CHANNEL_MAP_AMBISONIC_B_X &&
                    chanmap[2] == SF_CHANNEL_MAP_AMBISONIC_B_Y && chanmap[3] == SF_CHANNEL_MAP_AMBISONIC_B_Z)
                sconfig = SampleConfig_BFmt_WXYZ;
            else
            {
                sf_close(sndfile);
                return 0;
            }
        }
        else if(sf_command(sndfile, SFC_WAVEX_GET_AMBISONIC, 0, 0) == SF_AMBISONIC_B_FORMAT)
        {
            if(sndinfo.channels == 3)
                sconfig = SampleConfig_BFmt_WXY;
            else if(sndinfo.channels == 4)
                sconfig = SampleConfig_BFmt_WXYZ;
            else
            {
                sf_close(sndfile);
                return 0;
            }
        }
        else if(sndinfo.channels == 1)
            sconfig = SampleConfig_Mono;
        else if(sndinfo.channels == 2)
            sconfig = SampleConfig_Stereo;
        else
        {
            sf_close(sndfile);
            return 0;
        }
    }

    SampleType stype = SampleType_Int16;
    if(sndfile)
    {
        switch(sndinfo.format&SF_FORMAT_SUBMASK)
        {
            case SF_FORMAT_FLOAT:
            case SF_FORMAT_DOUBLE:
            case SF_FORMAT_VORBIS:
                stype = SampleType_Float32;
                break;

            default:
                stype = SampleType_Int16;
                break;
        }
    }

    if(!sndfile) return SharedPtr<Decoder>(nullptr);
    return SharedPtr<Decoder>(new SndFileDecoder(file, sndfile, sndinfo, sconfig, stype));
}

}
