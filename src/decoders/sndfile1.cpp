
#include "sndfile1.h"

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
    std::unique_ptr<std::istream> mFile;

    SNDFILE *mSndFile;
    SF_INFO mSndInfo;

public:
    SndFileDecoder(std::unique_ptr<std::istream> &&file, SNDFILE *sndfile, const SF_INFO sndinfo)
      : mFile(std::move(file)), mSndFile(sndfile), mSndInfo(sndinfo)
    { }
    virtual ~SndFileDecoder();

    virtual ALuint getFrequency() final;
    virtual SampleConfig getSampleConfig() final;
    virtual SampleType getSampleType() final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

SndFileDecoder::~SndFileDecoder()
{
    sf_close(mSndFile);
    mSndFile = 0;
}


ALuint SndFileDecoder::getFrequency()
{
    return mSndInfo.samplerate;
}

SampleConfig SndFileDecoder::getSampleConfig()
{
    if(mSndInfo.channels == 1)
        return SampleConfig_Mono;
    if(mSndInfo.channels == 2)
        return SampleConfig_Stereo;
    if(sf_command(mSndFile, SFC_WAVEX_GET_AMBISONIC, 0, 0) == SF_AMBISONIC_B_FORMAT)
    {
        if(mSndInfo.channels == 3)
            return SampleConfig_BFmt_WXY;
        if(mSndInfo.channels == 4)
            return SampleConfig_BFmt_WXYZ;
    }
    throw std::runtime_error("Unsupported sample configuration");
}

SampleType SndFileDecoder::getSampleType()
{
    std::cerr<< std::hex<<mSndInfo.format <<std::endl;
    return SampleType_Int16;
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

ALuint SndFileDecoder::read(ALvoid *ptr, ALuint count)
{
    sf_count_t got = sf_readf_short(mSndFile, static_cast<short*>(ptr), count);
    return (ALuint)std::max<sf_count_t>(got, 0);
}


Decoder *SndFileDecoderFactory::createDecoder(std::unique_ptr<std::istream> &file)
{
    SF_VIRTUAL_IO vio = {
        get_filelen, seek,
        read, write, tell
    };
    SF_INFO sndinfo;
    SNDFILE *sndfile = sf_open_virtual(&vio, SFM_READ, &sndinfo, file.get());
    if(!sndfile) return 0;
    return new SndFileDecoder(std::move(file), sndfile, sndinfo);
}

}
