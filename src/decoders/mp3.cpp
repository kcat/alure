
#include "mp3.hpp"

#include <stdexcept>
#include <iostream>
#include <cassert>

#include "context.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

namespace {

constexpr size_t MinMp3DataSize = 16384;
constexpr size_t MaxMp3DataSize = MinMp3DataSize * 8;

size_t append_file_data(std::istream &file, alure::Vector<uint8_t> &data, size_t count)
{
    size_t old_size = data.size();
    if(old_size >= MaxMp3DataSize || count == 0)
        return 0;
    count = std::min(count, MaxMp3DataSize - old_size);
    data.resize(old_size + count);

    file.clear();
    file.read(reinterpret_cast<char*>(data.data()+old_size), count);
    size_t got = file.gcount();
    data.resize(old_size + got);

    return got;
}

} // namespace


namespace alure {

class Mp3Decoder final : public Decoder {
    UniquePtr<std::istream> mFile;

    Vector<uint8_t> mFileData;

    mp3dec_t mMp3;
    Vector<short> mSampleData;

    ChannelConfig mChannels{ChannelConfig::Mono};
    SampleType mSampleType{SampleType::UInt8};
    int mSampleRate{0};

    static int decodeFrame(std::istream &file, mp3dec_t &mp3, Vector<uint8_t> &file_data,
                           short *sample_data, mp3dec_frame_info_t *frame_info)
    {
        if(file_data.size() < MinMp3DataSize)
        {
            size_t todo = MinMp3DataSize - file_data.size();
            if(append_file_data(file, file_data, todo) == 0)
                return 0;
        }

        int samples_to_get = mp3dec_decode_frame(&mp3, file_data.data(), file_data.size(),
                                                 sample_data, frame_info);
        while(samples_to_get == 0)
        {
            if(append_file_data(file, file_data, MinMp3DataSize) == 0)
                break;
            samples_to_get = mp3dec_decode_frame(&mp3, file_data.data(), file_data.size(),
                                                 sample_data, frame_info);
        }
        return samples_to_get;
    }

public:
    Mp3Decoder(UniquePtr<std::istream> file, Vector<uint8_t>&& initial_data,
               const mp3dec_t &mp3, ChannelConfig chans, SampleType stype, int srate) noexcept
      : mFile(std::move(file)), mFileData(std::move(initial_data)), mMp3(mp3)
      , mChannels(chans), mSampleType(stype), mSampleRate(srate)
    { }
    ~Mp3Decoder() override { }

    ALuint getFrequency() const noexcept override;
    ChannelConfig getChannelConfig() const noexcept override;
    SampleType getSampleType() const noexcept override;

    uint64_t getLength() const noexcept override;
    bool seek(uint64_t pos) noexcept override;

    std::pair<uint64_t,uint64_t> getLoopPoints() const noexcept override;

    ALuint read(ALvoid *ptr, ALuint count) noexcept override;
};

ALuint Mp3Decoder::getFrequency() const noexcept { return mSampleRate; }
ChannelConfig Mp3Decoder::getChannelConfig() const noexcept { return mChannels; }
SampleType Mp3Decoder::getSampleType() const noexcept { return mSampleType; }

uint64_t Mp3Decoder::getLength() const noexcept
{
    return 0;
}

bool Mp3Decoder::seek(uint64_t pos) noexcept
{
    // Seeking to somewhere in the file. Backup the current file position and
    // reset back to the beginning.
    // TODO: Obvious optimization: Track the current sample offset and don't
    // rewind if seeking forward.
    auto oldfpos = mFile->tellg();
    if(!mFile->seekg(0))
        return false;

    // Use temporary local storage to avoid trashing current data in case of
    // failure.
    Vector<uint8_t> file_data;
    mp3dec_t mp3 = mMp3;

    uint64_t curpos = 0;
    do {
        // Read the next frame.
        mp3dec_frame_info_t frame_info{};
        int samples_to_get = decodeFrame(*mFile, mp3, file_data, nullptr, &frame_info);
        if(samples_to_get <= 0) break;

        // Don't continue if the frame changed format
        if((mChannels == ChannelConfig::Mono   && frame_info.channels != 1) ||
           (mChannels == ChannelConfig::Stereo && frame_info.channels != 2) ||
           mSampleRate != frame_info.hz)
            break;

        if((uint64_t)samples_to_get > pos - curpos)
        {
            // Desired sample is within this frame, decode the samples and go
            // to the desired offset.
            Vector<short> sample_data(MINIMP3_MAX_SAMPLES_PER_FRAME);
            samples_to_get = decodeFrame(*mFile, mp3, file_data, sample_data.data(),
                                         &frame_info);

            if((uint64_t)samples_to_get > pos - curpos)
            {
                sample_data.resize(samples_to_get * frame_info.channels);
                sample_data.erase(sample_data.begin(),
                                  sample_data.begin() + (pos-curpos)*frame_info.channels);
                file_data.erase(file_data.begin(), file_data.begin()+frame_info.frame_bytes);
                mSampleData = std::move(sample_data);
                mFileData = std::move(file_data);
                mMp3 = mp3;
                return true;
            }
        }

        // Keep going to the next frame
        file_data.erase(file_data.begin(), file_data.begin()+frame_info.frame_bytes);
        curpos += samples_to_get;
    } while(1);

    // Seeking failed. Restore original file position.
    mFile->seekg(oldfpos);
    return false;
}

std::pair<uint64_t,uint64_t> Mp3Decoder::getLoopPoints() const noexcept
{
    return {0, std::numeric_limits<uint64_t>::max()};
}

ALuint Mp3Decoder::read(ALvoid *ptr, ALuint count) noexcept
{
    short *dst = reinterpret_cast<short*>(ptr);
    ALuint total = 0;

    while(total < count && mFile->good())
    {
        if(!mSampleData.empty())
        {
            // Write out whatever samples we have.
            ALuint todo = mSampleData.size();
            if(mChannels != ChannelConfig::Mono)
                todo /= 2;
            todo = std::min(todo, count-total);

            total += todo;

            if(mChannels != ChannelConfig::Mono) todo *= 2;
            std::copy(mSampleData.begin(), mSampleData.begin()+todo, dst);
            dst += todo;
            mSampleData.erase(mSampleData.begin(), mSampleData.begin()+todo);

            continue;
        }

        mSampleData.resize(MINIMP3_MAX_SAMPLES_PER_FRAME);

        mp3dec_frame_info_t frame_info{};
        int samples_to_get = decodeFrame(*mFile, mMp3, mFileData, mSampleData.data(),
                                         &frame_info);
        if(samples_to_get <= 0)
        {
            mSampleData.clear();
            break;
        }

        // Format changing not supported. End the stream.
        if((mChannels == ChannelConfig::Mono   && frame_info.channels != 1) ||
           (mChannels == ChannelConfig::Stereo && frame_info.channels != 2) ||
           mSampleRate != frame_info.hz)
        {
            mSampleData.clear();
            break;
        }

        // Remove used file data, update sample storage size with what we got
        mFileData.erase(mFileData.begin(), mFileData.begin()+frame_info.frame_bytes);
        mSampleData.resize(samples_to_get * frame_info.channels);
    }

    return total;
}


Mp3DecoderFactory::Mp3DecoderFactory() noexcept
{
}

Mp3DecoderFactory::~Mp3DecoderFactory()
{
}

SharedPtr<Decoder> Mp3DecoderFactory::createDecoder(UniquePtr<std::istream> &file) noexcept
{
    Vector<uint8_t> initial_data;
    mp3dec_t mp3{};

    mp3dec_init(&mp3);

    if(append_file_data(*file, initial_data, MinMp3DataSize) == 0)
        return nullptr;

    mp3dec_frame_info_t frame_info{};
    int samples_to_get = mp3dec_decode_frame(&mp3, initial_data.data(), initial_data.size(),
                                             nullptr, &frame_info);
    while(samples_to_get == 0)
    {
        if(append_file_data(*file, initial_data, MinMp3DataSize) == 0)
            break;

        samples_to_get = mp3dec_decode_frame(&mp3, initial_data.data(), initial_data.size(),
                                             nullptr, &frame_info);
    }
    if(!samples_to_get)
        return nullptr;

    if(frame_info.hz < 1)
        return nullptr;

    ChannelConfig chans = ChannelConfig::Mono;
    if(frame_info.channels == 1)
        chans = ChannelConfig::Mono;
    else if(frame_info.channels == 2)
        chans = ChannelConfig::Stereo;
    else
        return nullptr;

    SampleType stype = SampleType::Int16;
    // TODO: Fix MiniMP3 to output proper floats

    return MakeShared<Mp3Decoder>(std::move(file), std::move(initial_data), mp3,
                                  chans, stype, frame_info.hz);
}

} // namespace alure
