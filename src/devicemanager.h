#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class ALDevice;

class ALDeviceManager : public DeviceManager {
    Vector<UniquePtr<ALDevice>> mDevices;

    ALDeviceManager();
    virtual ~ALDeviceManager();

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static ALDeviceManager &get();

    void removeDevice(ALDevice *dev);

    bool queryExtension(const String &name) const override final;

    Vector<String> enumerate(DeviceEnumeration type) const override final;
    String defaultDeviceName(DefaultDeviceType type) const override final;

    Device openPlayback(const String &name) override final;
    Device openPlayback(const String &name, const std::nothrow_t&) override final;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
