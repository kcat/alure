#ifndef BUFFER_H
#define BUFFER_H

#include "main.h"

#include "al.h"

#include "refcount.h"

namespace alure {

class ALDevice;


ALuint FramesToBytes(ALuint size, SampleConfig chans, SampleType type);
ALenum GetFormat(SampleConfig chans, SampleType type);


class ALBuffer : public Buffer {
    ALDevice *const mDevice;
    ALuint mId;

    ALuint mFrequency;
    SampleConfig mSampleConfig;
    SampleType mSampleType;

    RefCount mRefs;

public:
    ALBuffer(ALDevice *device, ALuint id, ALuint freq, SampleConfig config, SampleType type)
      : mDevice(device), mId(id), mFrequency(freq), mSampleConfig(config), mSampleType(type), mRefs(0)
    { }
    virtual ~ALBuffer() { }

    void cleanup();

    unsigned long addRef() { return ++mRefs; }
    unsigned long decRef() { return --mRefs; }
    unsigned long getRef() { return mRefs.load(); }

    ALDevice *getDevice() { return mDevice; }
    const ALuint &getId() const { return mId; }

    virtual ALuint getLength() const final;

    virtual ALuint getFrequency() const final;
    virtual SampleConfig getSampleConfig() const final;
    virtual SampleType getSampleType() const final;

    virtual ALuint getSize() const final;

    virtual bool isInUse() const final;
};

} // namespace alure

#endif /* BUFFER_H */
