#ifndef AL_ALURE_H
#define AL_ALURE_H

#include <vector>
#include <string>

#include "alc.h"
#include "al.h"

namespace alure {

class Device;
class Context;
class DeviceManager;


enum DeviceEnumeration {
    DevEnum_Basic = ALC_DEVICE_SPECIFIER,
    DevEnum_Complete = ALC_ALL_DEVICES_SPECIFIER,
    DevEnum_Capture = ALC_CAPTURE_DEVICE_SPECIFIER
};

enum DefaultDeviceType {
    DefaultDevType_Basic = ALC_DEFAULT_DEVICE_SPECIFIER,
    DefaultDevType_Complete = ALC_DEFAULT_ALL_DEVICES_SPECIFIER,
    DefaultDevType_Capture = ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER
};

class DeviceManager {
protected:
    virtual ~DeviceManager() { }

public:
    static DeviceManager *get();

    virtual bool queryExtension(const char *extname) = 0;

    virtual std::vector<std::string> enumerate(DeviceEnumeration type) = 0;
    virtual std::string defaultDeviceName(DefaultDeviceType type) = 0;

    virtual Device *openPlayback(const std::string &name=std::string()) = 0;
};


enum PlaybackDeviceType {
    PlaybackDevType_Basic = ALC_DEVICE_SPECIFIER,
    PlaybackDevType_Complete = ALC_ALL_DEVICES_SPECIFIER
};

class Device {
protected:
    virtual ~Device() { }

public:
    virtual std::string getName(PlaybackDeviceType type) = 0;
    virtual bool queryExtension(const char *extname) = 0;

    virtual Context *createContext(ALCint *attribs=0) = 0;

    virtual void close() = 0;
};


class Context {
protected:
    virtual ~Context() { }

public:
    static bool MakeCurrent(Context *context);
    static Context *GetCurrent();

    virtual void destroy() = 0;

    virtual Device *getDevice() = 0;
};

} // namespace alure

#endif /* AL_ALURE_H */
