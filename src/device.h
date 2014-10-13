#ifndef DEVICE_H
#define DEVICE_H

#include "alure2.h"

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDevice : public Device {
    ALCdevice *mDevice;

    virtual ~ALDevice() { }
public:
    ALDevice(ALCdevice *device) : mDevice(device) { }

    virtual bool queryExtension(const char *extname) final;

    virtual void close() final;
};

} // namespace alure

#endif /* DEVICE_H */
