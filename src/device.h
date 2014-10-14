#ifndef DEVICE_H
#define DEVICE_H

#include "alure2.h"

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALContext;

class ALDevice : public Device {
    ALCdevice *mDevice;

    std::vector<ALContext*> mContexts;

    virtual ~ALDevice() { }
public:
    ALDevice(ALCdevice *device) : mDevice(device) { }

    ALCdevice *getDevice() const { return mDevice; }
    void removeContext(ALContext *ctx);

    virtual std::string getName(PlaybackDeviceType type) final;
    virtual bool queryExtension(const char *extname) final;

    virtual Context *createContext(ALCint *attribs=0) final;

    virtual void close() final;
};

} // namespace alure

#endif /* DEVICE_H */
