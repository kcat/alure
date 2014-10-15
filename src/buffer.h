#ifndef BUFFER_H
#define BUFFER_H

#include "alure2.h"

#include "al.h"

#include "refcount.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice;

class ALBuffer : public Buffer {
    ALDevice *const mDevice;
    ALuint mId;

    RefCount mRefs;

public:
    ALBuffer(ALDevice *device, ALuint id)
      : mDevice(device), mId(id), mRefs(0)
    { }

    void cleanup();

    long addRef() { return ++mRefs; }
    long decRef() { return ++mRefs; }
    long getRef() { return mRefs.load(); }

    const ALuint &getId() const { return mId; }

    virtual ALuint getSize() final;
};

} // namespace alure

#endif /* BUFFER_H */
