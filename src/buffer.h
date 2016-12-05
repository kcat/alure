#ifndef BUFFER_H
#define BUFFER_H

#include "main.h"

#include <algorithm>

#include "al.h"

#include "refcount.h"

namespace alure {

class ALContext;


ALuint FramesToBytes(ALuint size, ChannelConfig chans, SampleType type);
ALenum GetFormat(ChannelConfig chans, SampleType type);


class ALBuffer : public Buffer {
    ALContext *const mContext;
    ALuint mId;

    ALuint mFrequency;
    ChannelConfig mChannelConfig;
    SampleType mSampleType;

    BufferLoadStatus mLoadStatus;
    std::atomic<bool> mIsLoaded;

    Vector<Source*> mSources;

public:
    ALBuffer(ALContext *context, ALuint id, ALuint freq, ChannelConfig config, SampleType type, bool preloaded)
      : mContext(context), mId(id), mFrequency(freq), mChannelConfig(config), mSampleType(type),
        mLoadStatus(preloaded ? BufferLoadStatus::Ready : BufferLoadStatus::Pending),
        mIsLoaded(preloaded)
    { }
    virtual ~ALBuffer() { }

    void cleanup();

    ALContext *getContext() { return mContext; }
    ALuint getId() const { return mId; }

    void addSource(Source *source) { mSources.push_back(source); }
    void removeSource(Source *source)
    {
        auto iter = std::find(mSources.cbegin(), mSources.cend(), source);
        if(iter != mSources.cend()) mSources.erase(iter);
    }

    void load(ALuint frames, ALenum format, SharedPtr<Decoder> decoder, const String &name, ALContext *ctx);

    bool isReady() const { return mLoadStatus == BufferLoadStatus::Ready; }

    ALuint getLength() const override final;

    ALuint getFrequency() const override final { return mFrequency; }
    ChannelConfig getChannelConfig() const override final { return mChannelConfig; }
    SampleType getSampleType() const override final { return mSampleType; }

    ALuint getSize() const override final;

    void setLoopPoints(ALuint start, ALuint end) override final;
    std::pair<ALuint,ALuint> getLoopPoints() const override final;

    Vector<Source*> getSources() const override final { return mSources; }

    BufferLoadStatus getLoadStatus() override final;

    bool isInUse() const override final { return (mSources.size() > 0); }
};

} // namespace alure

#endif /* BUFFER_H */
