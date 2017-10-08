#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class ALDevice;

class ALDeviceManager {
    Vector<UniquePtr<ALDevice>> mDevices;

    ALDeviceManager();
    ~ALDeviceManager();

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static ALDeviceManager &get();

    void removeDevice(ALDevice *dev);

    bool queryExtension(const String &name) const;

    Vector<String> enumerate(DeviceEnumeration type) const;
    String defaultDeviceName(DefaultDeviceType type) const;

    Device openPlayback(const String &name);
    Device openPlayback(const String &name, const std::nothrow_t&);
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
