#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class ALDeviceManager : public DeviceManager {
    ALDeviceManager();

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static ALDeviceManager &get();

    virtual ~ALDeviceManager() { }

    virtual bool queryExtension(const char *extname) const final;

    virtual Vector<String> enumerate(DeviceEnumeration type) const final;
    virtual String defaultDeviceName(DefaultDeviceType type) const final;

    virtual Device *openPlayback(const String &name) final;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
