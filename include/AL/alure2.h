#ifndef AL_ALURE_H
#define AL_ALURE_H

#include <vector>
#include <string>

#include "alc.h"
#include "al.h"

namespace alure {

enum DeviceEnumeration {
    DevEnum_Basic = ALC_DEVICE_SPECIFIER,
    DevEnum_Complete = ALC_ALL_DEVICES_SPECIFIER,
    DevEnum_Capture = ALC_CAPTURE_DEVICE_SPECIFIER
};

enum PlaybackDeviceName {
    PlaybackDevName_Basic = ALC_DEVICE_SPECIFIER,
    PlaybackDevName_Complete = ALC_ALL_DEVICES_SPECIFIER
};

class Device {
protected:
    virtual ~Device() { }

public:
    virtual std::string getName(PlaybackDeviceName type) = 0;

    virtual bool queryExtension(const char *extname) = 0;

    virtual void close() = 0;
};

class DeviceManager {
protected:
    virtual ~DeviceManager() { }

public:
    static DeviceManager *get();

    virtual bool queryExtension(const char *extname) = 0;

    virtual std::vector<std::string> enumerate(DeviceEnumeration type) = 0;

    virtual Device *openPlayback(const std::string &name=std::string()) = 0;
};

} // namespace alure

#endif /* AL_ALURE_H */
