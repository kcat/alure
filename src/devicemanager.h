#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "main.h"

namespace alure {

class ALDeviceManager : public DeviceManager {
public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    ALDeviceManager();
    virtual ~ALDeviceManager() { }

    virtual bool queryExtension(const char *extname) const final;

    virtual std::vector<std::string> enumerate(DeviceEnumeration type) const final;
    virtual std::string defaultDeviceName(DefaultDeviceType type) const final;

    virtual Device *openPlayback(const std::string &name) final;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
