
#include "sndfile1.h"

#include <stdexcept>
#include <iostream>

#include "sndfile.h"

namespace alure
{

class SndFileDecoder : public Decoder {
    std::unique_ptr<std::istream> mFile;

    SNDFILE *mSndFile;
    SF_INFO mSndInfo;

public:
    SndFileDecoder(std::unique_ptr<std::istream> file);
    virtual ~SndFileDecoder();

    bool init();

    virtual ALuint getFrequency() final;
    virtual SampleConfig getSampleConfig() final;
    virtual SampleType getSampleType() final;

    virtual ALuint getLength() final;
    virtual ALuint getPosition() final;
    virtual bool seek(ALuint pos) final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;


    static sf_count_t get_filelen(void *user_data);
    static sf_count_t seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t tell(void *user_data);
};

SndFileDecoder::SndFileDecoder(std::unique_ptr<std::istream> file)
  : mFile(std::move(file)), mSndFile(0)
{ }

SndFileDecoder::~SndFileDecoder()
{
    if(mSndFile)
        sf_close(mSndFile);
    mSndFile = 0;
}

bool SndFileDecoder::init()
{
    SF_VIRTUAL_IO vio = {
        get_filelen,
        seek,
        read,
        write,
        tell
    };
    mSndFile = sf_open_virtual(&vio, SFM_READ, &mSndInfo, this);
    return !!mSndFile;
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
    return SampleType_Int16;
}


ALuint SndFileDecoder::getLength()
{
    return std::max<sf_count_t>(mSndInfo.frames, 0);
}

ALuint SndFileDecoder::getPosition()
{
    sf_count_t pos = sf_seek(mSndFile, 0, SEEK_CUR);
    return std::max<sf_count_t>(pos, 0);
}

bool SndFileDecoder::seek(ALuint pos)
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


sf_count_t SndFileDecoder::get_filelen(void *user_data)
{
    std::istream *file = reinterpret_cast<SndFileDecoder*>(user_data)->mFile.get();
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

sf_count_t SndFileDecoder::seek(sf_count_t offset, int whence, void *user_data)
{
    std::istream *file = reinterpret_cast<SndFileDecoder*>(user_data)->mFile.get();

    file->clear();
    if(!file->seekg(offset, std::ios::seekdir(whence)))
        return -1;
    return file->tellg();
}

sf_count_t SndFileDecoder::read(void *ptr, sf_count_t count, void *user_data)
{
    std::istream *file = reinterpret_cast<SndFileDecoder*>(user_data)->mFile.get();

    file->clear();
    file->read(reinterpret_cast<char*>(ptr), count);
    return file->gcount();
}

sf_count_t SndFileDecoder::write(const void*, sf_count_t, void*)
{
    return -1;
}

sf_count_t SndFileDecoder::tell(void* user_data)
{
    std::istream *file = reinterpret_cast<SndFileDecoder*>(user_data)->mFile.get();

    file->clear();
    return file->tellg();
}


Decoder *SndFileDecoderFactory::createDecoder(const std::string &name)
{
    std::unique_ptr<std::istream> file(FileIOFactory::get()->createFile(name).release());
    if(!file.get()) return 0;

    SndFileDecoder *decoder = new SndFileDecoder(std::move(file));
    if(!decoder->init())
    {
        delete decoder;
        decoder = 0;
    }
    return decoder;
}

}
