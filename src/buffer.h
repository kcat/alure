#ifndef BUFFER_H
#define BUFFER_H

#include "main.h"

#include <algorithm>

namespace alure {

ALenum GetFormat(ChannelConfig chans, SampleType type);

class BufferImpl {
    ContextImpl *const mContext;
    ALuint mId;

    ALuint mFrequency;
    ChannelConfig mChannelConfig;
    SampleType mSampleType;

    Vector<Source> mSources;

    const String mName;

public:
    BufferImpl(ContextImpl *context, ALuint id, ALuint freq, ChannelConfig config, SampleType type, StringView name)
      : mContext(context), mId(id), mFrequency(freq), mChannelConfig(config), mSampleType(type)
      , mName(String(name))
    { }

    void cleanup();

    ContextImpl *getContext() { return mContext; }
    ALuint getId() const { return mId; }

    void addSource(Source source) { mSources.push_back(source); }
    void removeSource(Source source)
    {
        auto iter = std::find(mSources.cbegin(), mSources.cend(), source);
        if(iter != mSources.cend()) mSources.erase(iter);
    }

    void load(ALuint frames, ALenum format, SharedPtr<Decoder> decoder, ContextImpl *ctx);

    ALuint getLength() const;

    ALuint getFrequency() const { return mFrequency; }
    ChannelConfig getChannelConfig() const { return mChannelConfig; }
    SampleType getSampleType() const { return mSampleType; }

    ALuint getSize() const;

    void setLoopPoints(ALuint start, ALuint end);
    std::pair<ALuint,ALuint> getLoopPoints() const;

    Vector<Source> getSources() const { return mSources; }

    const String &getName() const { return mName; }

    bool isInUse() const { return (mSources.size() > 0); }
};

} // namespace alure

#endif /* BUFFER_H */
