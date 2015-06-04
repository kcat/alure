
#include "config.h"

#include "devicemanager.h"
#include "device.h"

#include <string.h>

#include <stdexcept>

#include "alc.h"
#include "al.h"

namespace alure
{

template<typename T>
static inline void GetDeviceProc(T **func, ALCdevice *device, const char *name)
{ *func = reinterpret_cast<T*>(alcGetProcAddress(device, name)); }


ALCboolean (ALC_APIENTRY*ALDeviceManager::SetThreadContext)(ALCcontext*);

DeviceManager *DeviceManager::get()
{
    static ALDeviceManager singleton;
    return &singleton;
}

ALDeviceManager::ALDeviceManager()
{
    if(alcIsExtensionPresent(0, "ALC_EXT_thread_local_context"))
        GetDeviceProc(&SetThreadContext, 0, "alcSetThreadContext");
}

bool ALDeviceManager::queryExtension(const char *extname) const
{
    return alcIsExtensionPresent(0, extname);
}

std::vector<std::string> ALDeviceManager::enumerate(DeviceEnumeration type) const
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

std::string ALDeviceManager::defaultDeviceName(DefaultDeviceType type) const
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
    {
        if(name.empty())
            throw std::runtime_error("Failed to open default device");
        throw std::runtime_error("Failed to open device \""+name+"\"");
    }
    return new ALDevice(dev);
}

}
