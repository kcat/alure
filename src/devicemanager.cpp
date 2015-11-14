
#include "config.h"

#include "devicemanager.h"
#include "device.h"

#include <string.h>

#include <algorithm>
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
    return &ALDeviceManager::get();
}


ALDeviceManager &ALDeviceManager::get()
{
    static ALDeviceManager singleton;
    return singleton;
}


void ALDeviceManager::remove(ALDevice *device)
{
    auto iter = std::find(mDevices.begin(), mDevices.end(), device);
    if(iter != mDevices.end()) mDevices.erase(iter);
}


ALDeviceManager::ALDeviceManager() : mSingleCtxMode(false)
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


void ALDeviceManager::setSingleContextMode(bool enable)
{
    if(!mDevices.empty())
        throw std::runtime_error("Devices are open");

    if(enable)
        SetThreadContext = 0;
    else if(alcIsExtensionPresent(0, "ALC_EXT_thread_local_context"))
        GetDeviceProc(&SetThreadContext, 0, "alcSetThreadContext");
    mSingleCtxMode = enable;
}


Device *ALDeviceManager::openPlayback(const std::string &name)
{
    if(mSingleCtxMode && !mDevices.empty())
        throw std::runtime_error("Device already open");

    ALCdevice *dev = alcOpenDevice(name.c_str());
    if(!dev)
    {
        if(name.empty())
            throw std::runtime_error("Failed to open default device");
        throw std::runtime_error("Failed to open device \""+name+"\"");
    }
    mDevices.push_back(new ALDevice(dev));
    return mDevices.back();
}

}
