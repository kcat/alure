
#include "flac.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>

#include "FLAC/all.h"


namespace alure
{

class FlacDecoder : public Decoder {
    UniquePtr<std::istream> mFile;

    FLAC__StreamDecoder *mFlacFile;
    ChannelConfig mChannelConfig;
    SampleType mSampleType;
    ALuint mFrequency;
    ALuint mFrameSize;
    uint64_t mSamplePos;

    Vector<ALubyte> mData;

    ALubyte *mOutBytes;
    ALuint mOutMax;
    ALuint mOutLen;

    void CopySamples(ALubyte *output, ALuint todo, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], ALuint offset)
    {
        if(mSampleType == SampleType::UInt8)
        {
            ALubyte *samples = output;
            for(ALuint c = 0;c < frame->header.channels;c++)
            {
                for(ALuint i = 0;i < todo;i++)
                    samples[i*frame->header.channels + c] = ALubyte(buffer[c][offset+i] + 0x80);
            }
        }
        else
        {
            int shift = frame->header.bits_per_sample - 16;
            ALshort *samples = reinterpret_cast<ALshort*>(output);
            for(ALuint c = 0;c < frame->header.channels;c++)
            {
                for(ALuint i = 0;i < todo;i++)
                    samples[i*frame->header.channels + c] = buffer[c][offset+i] >> shift;
            }
        }
    }

    static FLAC__StreamDecoderWriteStatus WriteCallback(const FLAC__StreamDecoder*, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
    {
        FlacDecoder *self = static_cast<FlacDecoder*>(client_data);

        if(self->mFrequency == 0)
        {
            ALuint bps = frame->header.bits_per_sample;
            if(bps == 8)
                self->mSampleType = SampleType::UInt8;
            else
            {
                self->mSampleType = SampleType::Int16;
                bps = 16;
            }

            if(frame->header.channels == 1)
                self->mChannelConfig = ChannelConfig::Mono;
            else if(frame->header.channels == 2)
                self->mChannelConfig = ChannelConfig::Stereo;
            else
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

            self->mFrameSize = frame->header.channels * bps/8;
            self->mFrequency = frame->header.sample_rate;
        }

        ALubyte *data = self->mOutBytes + self->mOutLen;
        ALuint todo = std::min<ALuint>((self->mOutMax-self->mOutLen) / self->mFrameSize,
                                       frame->header.blocksize);
        self->CopySamples(data, todo, frame, buffer, 0);
        self->mOutLen += self->mFrameSize * todo;

        if(todo < frame->header.blocksize)
        {
            ALuint offset = todo;
            todo = frame->header.blocksize - todo;

            ALuint blocklen = todo * self->mFrameSize;
            ALuint start = self->mData.size();

            self->mData.resize(start+blocklen);
            data = &self->mData[start];

            self->CopySamples(data, todo, frame, buffer, offset);
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }
    static void MetadataCallback(const FLAC__StreamDecoder*,const FLAC__StreamMetadata*,void*)
    {
    }
    static void ErrorCallback(const FLAC__StreamDecoder*,FLAC__StreamDecoderErrorStatus,void*)
    {
    }

    static FLAC__StreamDecoderReadStatus ReadCallback(const FLAC__StreamDecoder*, FLAC__byte buffer[], size_t *bytes, void *client_data)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        stream->clear();

        if(*bytes <= 0)
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

        stream->read(reinterpret_cast<char*>(buffer), *bytes);
        *bytes = stream->gcount();
        if(*bytes == 0 && stream->eof())
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

        return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
    }
    static FLAC__StreamDecoderSeekStatus SeekCallback(const FLAC__StreamDecoder*, FLAC__uint64 absolute_byte_offset, void *client_data)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        stream->clear();

        if(!stream->seekg(absolute_byte_offset))
            return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }
    static FLAC__StreamDecoderTellStatus TellCallback(const FLAC__StreamDecoder*, FLAC__uint64 *absolute_byte_offset, void *client_data)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        stream->clear();

        *absolute_byte_offset = stream->tellg();
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }
    static FLAC__StreamDecoderLengthStatus LengthCallback(const FLAC__StreamDecoder*, FLAC__uint64 *stream_length, void *client_data)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        stream->clear();

        std::streampos pos = stream->tellg();
        if(stream->seekg(0, std::ios_base::end))
        {
            *stream_length = stream->tellg();
            stream->seekg(pos);
        }

        if(!stream->good())
            return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
        return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
    }
    static FLAC__bool EofCallback(const FLAC__StreamDecoder*, void *client_data)
    {
        std::istream *stream = static_cast<FlacDecoder*>(client_data)->mFile.get();
        return stream->eof() ? true : false;
    }

public:
    FlacDecoder()
      : mFlacFile(nullptr), mChannelConfig(ChannelConfig::Mono), mSampleType(SampleType::Int16)
      , mFrequency(0), mFrameSize(0), mSamplePos(0), mOutBytes(nullptr), mOutMax(0), mOutLen(0)
    { }
    virtual ~FlacDecoder();

    bool open(UniquePtr<std::istream> &file);

    virtual ALuint getFrequency() const final;
    virtual ChannelConfig getChannelConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual uint64_t getLength() final;
    virtual uint64_t getPosition() final;
    virtual bool seek(uint64_t pos) final;

    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;
};

FlacDecoder::~FlacDecoder()
{
    if(mFlacFile)
    {
        FLAC__stream_decoder_finish(mFlacFile);
        FLAC__stream_decoder_delete(mFlacFile);
        mFlacFile = nullptr;
    }
}


bool FlacDecoder::open(UniquePtr<std::istream> &file)
{
    mFlacFile = FLAC__stream_decoder_new();
    if(mFlacFile)
    {
        mFile = std::move(file);
        if(FLAC__stream_decoder_init_stream(mFlacFile, ReadCallback, SeekCallback, TellCallback, LengthCallback, EofCallback, WriteCallback, MetadataCallback, ErrorCallback, this) == FLAC__STREAM_DECODER_INIT_STATUS_OK)
        {
            while(mData.empty())
            {
                if(FLAC__stream_decoder_process_single(mFlacFile) == false ||
                   FLAC__stream_decoder_get_state(mFlacFile) == FLAC__STREAM_DECODER_END_OF_STREAM)
                    break;
            }
            if(!mData.empty())
                return true;

            FLAC__stream_decoder_finish(mFlacFile);
        }
        FLAC__stream_decoder_delete(mFlacFile);
        mFlacFile = nullptr;

        file = std::move(mFile);
    }

    return false;
}


ALuint FlacDecoder::getFrequency() const
{
    return mFrequency;
}

ChannelConfig FlacDecoder::getChannelConfig() const
{
    return mChannelConfig;
}

SampleType FlacDecoder::getSampleType() const
{
    return mSampleType;
}


uint64_t FlacDecoder::getLength()
{
    return FLAC__stream_decoder_get_total_samples(mFlacFile);
}

uint64_t FlacDecoder::getPosition()
{
    return mSamplePos;
}

bool FlacDecoder::seek(uint64_t pos)
{
    if(!FLAC__stream_decoder_seek_absolute(mFlacFile, pos))
        return false;
    mSamplePos = pos;
    return true;
}

std::pair<uint64_t,uint64_t> FlacDecoder::getLoopPoints() const
{
    return std::make_pair(0, 0);
}

ALuint FlacDecoder::read(ALvoid *ptr, ALuint count)
{
    mOutBytes = reinterpret_cast<ALubyte*>(ptr);
    mOutLen = 0;
    mOutMax = count * mFrameSize;

    if(mData.size() > 0)
    {
        size_t rem = std::min(mData.size(), (size_t)mOutMax);
        memcpy(ptr, &mData[0], rem);
        mOutLen += rem;
        mData.erase(mData.begin(), mData.begin()+rem);
    }

    while(mOutLen < mOutMax)
    {
        if(FLAC__stream_decoder_process_single(mFlacFile) == false ||
           FLAC__stream_decoder_get_state(mFlacFile) == FLAC__STREAM_DECODER_END_OF_STREAM)
            break;
    }
    ALuint ret = mOutLen / mFrameSize;

    mSamplePos += ret;

    return ret;
}


SharedPtr<Decoder> FlacDecoderFactory::createDecoder(UniquePtr<std::istream> &file)
{
    auto decoder = MakeShared<FlacDecoder>();
    if(!decoder->open(file)) decoder.reset();
    return decoder;
}

}
