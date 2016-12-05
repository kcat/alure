#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class ALDeviceManager : public DeviceManager {
    ALDeviceManager();

public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    static ALDeviceManager &get();

    bool queryExtension(const char *extname) const override final;

    Vector<String> enumerate(DeviceEnumeration type) const override final;
    String defaultDeviceName(DefaultDeviceType type) const override final;

    Device *openPlayback(const String &name) override final;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
