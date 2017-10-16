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

    bool queryExtension(StringView name) const;

    Vector<String> enumerate(DeviceEnumeration type) const;
    String defaultDeviceName(DefaultDeviceType type) const;

    Device openPlayback(StringView name);
    Device openPlayback(StringView name, const std::nothrow_t&);
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
