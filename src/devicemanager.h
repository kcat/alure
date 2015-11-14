#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class ALDevice;

class ALDeviceManager : public DeviceManager {
    std::vector<ALDevice*> mDevices;
    bool mSingleCtxMode;

    ALDeviceManager();

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static ALDeviceManager &get();

    void remove(ALDevice *device);

    virtual ~ALDeviceManager() { }

    virtual bool queryExtension(const char *extname) const final;

    virtual std::vector<std::string> enumerate(DeviceEnumeration type) const final;
    virtual std::string defaultDeviceName(DefaultDeviceType type) const final;

    virtual void setSingleContextMode(bool enable) final;
    virtual bool getSingleContextMode() const final { return mSingleCtxMode; }

    virtual Device *openPlayback(const std::string &name) final;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
