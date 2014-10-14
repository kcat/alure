#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "alure2.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALDeviceManager : public DeviceManager {
public:
    static ALCboolean (ALC_APIENTRY*SetThreadContext)(ALCcontext*);

    ALDeviceManager();
    virtual ~ALDeviceManager() { }

    virtual bool queryExtension(const char *extname) final;

    virtual std::vector<std::string> enumerate(DeviceEnumeration type) final;
    virtual std::string defaultDeviceName(DefaultDeviceType type) final;

    virtual Device *openPlayback(const std::string &name) final;
};

} // namespace alure

#endif /* DEVICEMANAGER_H */
