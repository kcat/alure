
#include "mpg123-1.h"

#include <stdexcept>
#include <iostream>

#include "mpg123.h"

namespace alure
{

class Mpg123Decoder : public Decoder {
    std::unique_ptr<std::istream> mFile;

    mpg123_handle *mMpg123;
    int mChannels;
    long mSampleRate;

public:
    Mpg123Decoder(std::unique_ptr<std::istream> file);
    virtual ~Mpg123Decoder();

    bool init();

    virtual ALuint getFrequency() final;
    virtual SampleConfig getSampleConfig() final;
    virtual SampleType getSampleType() final;

    virtual ALuint getLength() final;
    virtual ALuint getPosition() final;
    virtual bool seek(ALuint pos) final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;

    static ssize_t r_read(void *user_data, void *ptr, size_t count);
    static off_t r_lseek(void *user_data, off_t offset, int whence);
};

Mpg123Decoder::Mpg123Decoder(std::unique_ptr<std::istream> file)
  : mFile(std::move(file)), mMpg123(0)
{ }

Mpg123Decoder::~Mpg123Decoder()
{
    if(mMpg123)
    {
        mpg123_close(mMpg123);
        mpg123_delete(mMpg123);
        mMpg123 = 0;
    }
}

bool Mpg123Decoder::init()
{
    mMpg123 = mpg123_new(0, 0);
    if(mMpg123)
    {
        if(mpg123_replace_reader_handle(mMpg123, r_read, r_lseek, 0) == MPG123_OK &&
           mpg123_open_handle(mMpg123, this) == MPG123_OK)
        {
            int enc, channels;
            long srate;

            if(mpg123_getformat(mMpg123, &srate, &channels, &enc) == MPG123_OK)
            {
                if((channels == 1 || channels == 2) && srate > 0 &&
                   mpg123_format_none(mMpg123) == MPG123_OK &&
                   mpg123_format(mMpg123, srate, channels, MPG123_ENC_SIGNED_16) == MPG123_OK)
                {
                    // All OK
                    mChannels = channels;
                    mSampleRate = srate;
                    return true;
                }
            }
        }
    }
    return false;
}


ALuint Mpg123Decoder::getFrequency()
{
    return mSampleRate;
}

SampleConfig Mpg123Decoder::getSampleConfig()
{
    if(mChannels == 1)
        return SampleConfig_Mono;
    if(mChannels == 2)
        return SampleConfig_Stereo;
    throw std::runtime_error("Unsupported sample configuration");
}

SampleType Mpg123Decoder::getSampleType()
{
    return SampleType_Int16;
}


ALuint Mpg123Decoder::getLength()
{
    off_t len = mpg123_length(mMpg123);
    return (ALuint)std::max<off_t>(len, 0);
}

ALuint Mpg123Decoder::getPosition()
{
    off_t pos = mpg123_tell(mMpg123);
    return (ALuint)std::max<off_t>(pos, 0);
}

bool Mpg123Decoder::seek(ALuint pos)
{
    off_t newpos = mpg123_seek(mMpg123, pos, SEEK_SET);
    if(newpos < 0) return false;
    return true;
}

ALuint Mpg123Decoder::read(ALvoid *ptr, ALuint count)
{
    ALuint total = 0;
    while(total < count)
    {
        size_t got = 0;
        int ret = mpg123_read(mMpg123, reinterpret_cast<unsigned char*>(ptr), count*mChannels*2, &got);
        if((ret != MPG123_OK && ret != MPG123_DONE) || got == 0)
            break;
        total += got / mChannels / 2;
        if(ret == MPG123_DONE)
            break;
    }
    return total;
}


ssize_t Mpg123Decoder::r_read(void *user_data, void *ptr, size_t count)
{
    std::istream *file = reinterpret_cast<Mpg123Decoder*>(user_data)->mFile.get();

    file->clear();
    file->read(reinterpret_cast<char*>(ptr), count);
    return file->gcount();
}

off_t Mpg123Decoder::r_lseek(void* user_data, off_t offset, int whence)
{
    std::istream *file = reinterpret_cast<Mpg123Decoder*>(user_data)->mFile.get();

    file->clear();
    if(!file->seekg(offset, std::ios::seekdir(whence)))
        return -1;
    return file->tellg();
}


Decoder *Mpg123DecoderFactory::createDecoder(const std::string &name)
{
    static bool inited = false;
    if(!inited)
    {
        if(mpg123_init() != MPG123_OK)
            return 0;
        inited = true;
    }

    std::unique_ptr<std::istream> file(FileIOFactory::get()->createFile(name).release());
    if(!file.get()) return 0;

    Mpg123Decoder *decoder = new Mpg123Decoder(std::move(file));
    if(!decoder->init())
    {
        delete decoder;
        decoder = 0;
    }
    return decoder;
}

}
