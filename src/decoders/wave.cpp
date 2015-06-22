
#include "wave.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include "buffer.h"


namespace alure
{

static const ALubyte SUBTYPE_PCM[] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71
};
static const ALubyte SUBTYPE_FLOAT[] = {
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa,
    0x00, 0x38, 0x9b, 0x71
};

static const ALubyte SUBTYPE_BFORMAT_PCM[] = {
    0x01, 0x00, 0x00, 0x00, 0x21, 0x07, 0xd3, 0x11, 0x86, 0x44, 0xc8, 0xc1,
    0xca, 0x00, 0x00, 0x00
};
static const ALubyte SUBTYPE_BFORMAT_FLOAT[] = {
    0x03, 0x00, 0x00, 0x00, 0x21, 0x07, 0xd3, 0x11, 0x86, 0x44, 0xc8, 0xc1,
    0xca, 0x00, 0x00, 0x00
};

static const int CHANNELS_MONO       = 0x04;
static const int CHANNELS_STEREO     = 0x01 | 0x02;
static const int CHANNELS_QUAD       = 0x01 | 0x02               | 0x10 | 0x20;
static const int CHANNELS_5DOT1      = 0x01 | 0x02 | 0x04 | 0x08                       | 0x200 | 0x400;
static const int CHANNELS_5DOT1_REAR = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20;
static const int CHANNELS_6DOT1      = 0x01 | 0x02 | 0x04 | 0x08               | 0x100 | 0x200 | 0x400;
static const int CHANNELS_7DOT1      = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x200         | 0x400;


static ALuint read_le32(std::istream &stream)
{
    char buf[4];
    if(!stream.read(buf, sizeof(buf)) || stream.gcount() != sizeof(buf))
        return 0;
    return ((ALuint(buf[0]    )&0x000000ff) | (ALuint(buf[1]<< 8)&0x0000ff00) |
            (ALuint(buf[2]<<16)&0x00ff0000) | (ALuint(buf[3]<<24)&0xff000000));
}

static ALushort read_le16(std::istream &stream)
{
    char buf[2];
    if(!stream.read(buf, sizeof(buf)) || stream.gcount() != sizeof(buf))
        return 0;
    return ((ALushort(buf[0]   )&0x00ff) | (ALushort(buf[1]<<8)&0xff00));
}


class WaveDecoder : public Decoder {
    SharedPtr<std::istream> mFile;

    SampleConfig mSampleConfig;
    SampleType mSampleType;
    ALuint mFrequency;
    ALuint mFrameSize;

    // In sample frames, relative to sample data start
    std::pair<ALuint,ALuint> mLoopPts;

    // In bytes from beginning of file
    std::istream::pos_type mStart, mEnd;

public:
    WaveDecoder(SharedPtr<std::istream> file, SampleConfig channels, SampleType type, ALuint frequency, ALuint framesize,
                std::istream::pos_type start, std::istream::pos_type end, ALuint loopstart, ALuint loopend)
      : mFile(file), mSampleConfig(channels), mSampleType(type), mFrequency(frequency), mFrameSize(framesize),
        mLoopPts{loopstart,loopend}, mStart(start), mEnd(end)
    { }
    virtual ~WaveDecoder();

    virtual ALuint getFrequency() const final;
    virtual SampleConfig getSampleConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

WaveDecoder::~WaveDecoder()
{
}


ALuint WaveDecoder::getFrequency() const
{
    return mFrequency;
}

SampleConfig WaveDecoder::getSampleConfig() const
{
    return mSampleConfig;
}

SampleType WaveDecoder::getSampleType() const
{
    return mSampleType;
}


uint64_t WaveDecoder::getLength()
{
    return (mEnd - mStart) / mFrameSize;
}

uint64_t WaveDecoder::getPosition()
{
    mFile->clear();
    return (std::max(mFile->tellg(), mStart) - mStart) / mFrameSize;
}

bool WaveDecoder::seek(uint64_t pos)
{
    std::streamsize offset = pos*mFrameSize + mStart;
    mFile->clear();
    if(offset > mEnd || !mFile->seekg(offset))
        return false;
    return true;
}

std::pair<uint64_t,uint64_t> WaveDecoder::getLoopPoints() const
{
    return mLoopPts;
}

ALuint WaveDecoder::read(ALvoid *ptr, ALuint count)
{
    mFile->clear();

    auto pos = mFile->tellg();
    size_t len = count * mFrameSize;
    ALuint total = 0;

    if(pos < mEnd)
    {
        len = std::min<std::istream::pos_type>(len, mEnd-pos);
#ifdef __BIG_ENDIAN__
        switch(mSampleType)
        {
            case SampleType_Float32:
                while(total < len && mFile->good() && !mFile->eof())
                {
                    char temp[256];
                    size_t todo = std::min(len-total, sizeof(temp));

                    mFile->read(temp, todo);
                    std::streamsize got = mFile->gcount();

                    for(std::streamsize i = 0;i < got;++i)
                        reinterpret_cast<char*>(ptr)[total+i] = temp[i^3];

                    total += got;
                }
                total /= mFrameSize;
                break;

            case SampleType_Int16:
                while(total < len && mFile->good() && !mFile->eof())
                {
                    char temp[256];
                    size_t todo = std::min(len-total, sizeof(temp));

                    mFile->read(temp, todo);
                    std::streamsize got = mFile->gcount();

                    for(std::streamsize i = 0;i < got;++i)
                        reinterpret_cast<char*>(ptr)[total+i] = temp[i^1];

                    total += got;
                }
                total /= mFrameSize;
                break;

            case SampleType_UInt8:
            case SampleType_Mulaw:
#else
        {
#endif
                mFile->read(reinterpret_cast<char*>(ptr), len);
                total = mFile->gcount() / mFrameSize;
        }
    }

    return total;
}


SharedPtr<Decoder> WaveDecoderFactory::createDecoder(SharedPtr<std::istream> file)
{
    SampleConfig channels = SampleConfig_Mono;
    SampleType type = SampleType_UInt8;
    ALuint frequency = 0;
    ALuint framesize = 0;
    ALuint loop_pts[2]{0, 0};
    ALuint blockalign = 0;
    ALuint framealign = 0;

    char tag[4]{};
    if(!file->read(tag, 4) || file->gcount() != 4 || memcmp(tag, "RIFF", 4) != 0)
        return SharedPtr<Decoder>(nullptr);
    ALuint totalsize = read_le32(*file) & ~1u;
    if(!file->read(tag, 4) || file->gcount() != 4 || memcmp(tag, "WAVE", 4) != 0)
        return SharedPtr<Decoder>(nullptr);

    while(file->good() && !file->eof() && totalsize > 8)
    {
        if(!file->read(tag, 4) || file->gcount() != 4)
            return SharedPtr<Decoder>(nullptr);
        ALuint size = read_le32(*file);
        if(size < 2)
            return SharedPtr<Decoder>(nullptr);
        totalsize -= 8;
        size = std::min((size+1) & ~1u, totalsize);
        totalsize -= size;

        if(memcmp(tag, "fmt ", 4) == 0)
        {
            /* 'fmt ' tag needs at least 16 bytes. */
            if(size < 16) goto next_chunk;

            /* format type */
            ALushort fmttype = read_le16(*file); size -= 2;

            /* mono or stereo data */
            int chancount = read_le16(*file); size -= 2;

            /* sample frequency */
            frequency = read_le32(*file); size -= 4;

            /* skip average bytes per second */
            read_le32(*file); size -= 4;

            /* bytes per block */
            blockalign = read_le16(*file); size -= 2;

            /* bits per sample */
            int bitdepth = read_le16(*file); size -= 2;

            /* Look for any extra data and try to find the format */
            ALuint extrabytes = 0;
            if(size >= 2)
            {
                extrabytes = read_le16(*file);
                size -= 2;
            }
            extrabytes = std::min<ALuint>(extrabytes, size);

            /* Format type should be 0x0001 for integer PCM data, 0x0003 for
             * float PCM data, 0x0007 for muLaw, and 0xFFFE extensible data.
             */
            if(fmttype == 0x0001)
            {
                if(chancount == 1)
                    channels = SampleConfig_Mono;
                else if(chancount == 2)
                    channels = SampleConfig_Stereo;
                else
                    goto next_chunk;

                if(bitdepth == 8)
                    type = SampleType_UInt8;
                else if(bitdepth == 16)
                    type = SampleType_Int16;
                else
                    goto next_chunk;
            }
            else if(fmttype == 0x0003)
            {
                if(chancount == 1)
                    channels = SampleConfig_Mono;
                else if(chancount == 2)
                    channels = SampleConfig_Stereo;
                else
                    goto next_chunk;

                if(bitdepth == 32)
                    type = SampleType_Float32;
                else
                    goto next_chunk;
            }
            else if(fmttype == 0x0007)
            {
                if(chancount == 1)
                    channels = SampleConfig_Mono;
                else if(chancount == 2)
                    channels = SampleConfig_Stereo;
                else
                    goto next_chunk;

                if(bitdepth == 8)
                    type = SampleType_Mulaw;
                else
                    goto next_chunk;
            }
            else if(fmttype == 0xFFFE)
            {
                if(size < 22)
                    goto next_chunk;

                char subtype[16];
                ALushort validbits = read_le16(*file); size -= 2;
                ALuint chanmask = read_le32(*file); size -= 4;
                file->read(subtype, 16); size -= file->gcount();

                /* Padded bit depths not supported */
                if(validbits != bitdepth)
                    goto next_chunk;

                if(memcmp(subtype, SUBTYPE_BFORMAT_PCM, 16) == 0 || memcmp(subtype, SUBTYPE_BFORMAT_FLOAT, 16) == 0)
                {
                    if(chanmask != 0)
                        goto next_chunk;

                    if(chancount == 3)
                        channels = SampleConfig_BFmt_WXY;
                    else if(chancount == 4)
                        channels = SampleConfig_BFmt_WXYZ;
                    else
                        goto next_chunk;
                }
                else if(memcmp(subtype, SUBTYPE_PCM, 16) == 0 || memcmp(subtype, SUBTYPE_FLOAT, 16) == 0)
                {
                    if(chancount == 1 && chanmask == CHANNELS_MONO)
                        channels = SampleConfig_Mono;
                    else if(chancount == 2 && chanmask == CHANNELS_STEREO)
                        channels = SampleConfig_Stereo;
                    else if(chancount == 4 && chanmask == CHANNELS_QUAD)
                        channels = SampleConfig_Quad;
                    else if(chancount == 6 && (chanmask == CHANNELS_5DOT1 || chanmask == CHANNELS_5DOT1_REAR))
                        channels = SampleConfig_X51;
                    else if(chancount == 7 && chanmask == CHANNELS_6DOT1)
                        channels = SampleConfig_X61;
                    else if(chancount == 8 && chanmask == CHANNELS_7DOT1)
                        channels = SampleConfig_X71;
                    else
                        goto next_chunk;
                }

                if(memcmp(subtype, SUBTYPE_PCM, 16) == 0 || memcmp(subtype, SUBTYPE_BFORMAT_PCM, 16) == 0)
                {
                    if(bitdepth == 8)
                        type = SampleType_UInt8;
                    else if(bitdepth == 16)
                        type = SampleType_Int16;
                    else
                        goto next_chunk;
                }
                else if(memcmp(subtype, SUBTYPE_FLOAT, 16) == 0 || memcmp(subtype, SUBTYPE_BFORMAT_FLOAT, 16) == 0)
                {
                    if(bitdepth == 32)
                        type = SampleType_Float32;
                    else
                        goto next_chunk;
                }
                else
                    goto next_chunk;
            }
            else
                goto next_chunk;

            framesize = FramesToBytes(1, channels, type);

            /* Calculate the number of frames per block (ADPCM will need extra
             * consideration). */
            framealign = blockalign / framesize;
        }
        else if(memcmp(tag, "smpl", 4) == 0)
        {
            /* sampler data needs at least 36 bytes */
            if(size < 36) goto next_chunk;

            /* Most of this only affects MIDI sampling, but we only care about
             * the loop definitions at the end. */
            /*ALuint manufacturer =*/ read_le32(*file);
            /*ALuint product =*/ read_le32(*file);
            /*ALuint smpperiod =*/ read_le32(*file);
            /*ALuint unitynote =*/ read_le32(*file);
            /*ALuint pitchfrac =*/ read_le32(*file);
            /*ALuint smptefmt =*/ read_le32(*file);
            /*ALuint smpteoffset =*/ read_le32(*file);
            ALuint loopcount = read_le32(*file);
            /*ALuint extrabytes =*/ read_le32(*file);
            size -= 36;

            for(ALuint i = 0;i < loopcount && size >= 24;++i)
            {
                /*ALuint id =*/ read_le32(*file);
                ALuint type = read_le32(*file);
                ALuint loopstart = read_le32(*file);
                ALuint loopend = read_le32(*file);
                /*ALuint frac =*/ read_le32(*file);
                ALuint numloops = read_le32(*file);
                size -= 24;

                /* Only handle indefinite forward loops. */
                if(type == 0 || numloops == 0)
                {
                    loop_pts[0] = loopstart;
                    loop_pts[1] = loopend;
                    break;
                }
            }
        }
        else if(memcmp(tag, "data", 4) == 0)
        {
            if(framesize == 0)
                goto next_chunk;

            /* Make sure there's at least one sample frame of audio data. */
            std::istream::pos_type start = file->tellg();
            std::istream::pos_type end = start + std::istream::pos_type(size - (size%framesize));
            if(end-start >= framesize)
            {
                /* Loop points are byte offsets relative to the data start.
                 * Convert to sample frame offsets. */
                return SharedPtr<Decoder>(new WaveDecoder(file,
                    channels, type, frequency, framesize, start, end,
                    loop_pts[0] / blockalign * framealign,
                    loop_pts[1] / blockalign * framealign
                ));
            }
        }

    next_chunk:
        if(size > 0)
            file->ignore(size);
    }

    return SharedPtr<Decoder>(nullptr);
}

}
