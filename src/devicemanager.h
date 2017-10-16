#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class DeviceImpl;

class DeviceManagerImpl {
    Vector<UniquePtr<DeviceImpl>> mDevices;

    DeviceManagerImpl();
    ~DeviceManagerImpl();

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static DeviceManagerImpl &get();

    void removeDevice(DeviceImpl *dev);

    bool queryExtension(const char *name) const;

    Vector<String> enumerate(DeviceEnumeration type) const;
    String defaultDeviceName(DefaultDeviceType type) const;

    Device openPlayback(const char *name);
    Device openPlayback(const char *name, const std::nothrow_t&);
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
