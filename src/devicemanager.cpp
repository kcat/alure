
#include "devicemanager.h"
#include "device.h"

#include <string.h>

#include <stdexcept>

#include "alc.h"
#include "al.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure
{

DeviceManager *DeviceManager::get()
{
    static ALDeviceManager singleton;
    return &singleton;
}

bool ALDeviceManager::queryExtension(const char *extname)
{
    return alcIsExtensionPresent(0, extname);
}

std::vector<std::string> ALDeviceManager::enumerate(DeviceEnumeration type)
{
    std::vector<std::string> list;
    if(type == DevEnum_Complete && !alcIsExtensionPresent(0, "ALC_ENUMERATE_ALL_EXT"))
        type = DevEnum_Basic;
    const ALCchar *names = alcGetString(0, type);
    while(names && *names)
    {
        list.push_back(names);
        names += strlen(names)+1;
    }
    return list;
}

std::string ALDeviceManager::defaultDeviceName(DefaultDeviceType type)
{
    if(type == DefaultDevType_Complete && !alcIsExtensionPresent(0, "ALC_ENUMERATE_ALL_EXT"))
        type = DefaultDevType_Basic;
    const ALCchar *name = alcGetString(0, type);
    return std::string(name ? name : "");
}

Device *ALDeviceManager::openPlayback(const std::string &name)
{
    ALCdevice *dev = alcOpenDevice(name.c_str());
    if(!dev)
        throw std::runtime_error("Failed to open \""+name+"\"");
    return new ALDevice(dev);
}

}
