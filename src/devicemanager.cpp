
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

DeviceManager &DeviceManager::get()
{
    return ALDeviceManager::get();
}


ALDeviceManager &ALDeviceManager::get()
{
    static ALDeviceManager singleton;
    return singleton;
}


ALDeviceManager::ALDeviceManager()
{
    if(alcIsExtensionPresent(0, "ALC_EXT_thread_local_context"))
        GetDeviceProc(&SetThreadContext, 0, "alcSetThreadContext");
}

ALDeviceManager::~ALDeviceManager()
{
}


bool ALDeviceManager::queryExtension(const String &name) const
{
    return alcIsExtensionPresent(nullptr, name.c_str());
}

Vector<String> ALDeviceManager::enumerate(DeviceEnumeration type) const
{
    Vector<String> list;
    if(type == DeviceEnumeration::Complete && !alcIsExtensionPresent(0, "ALC_ENUMERATE_ALL_EXT"))
        type = DeviceEnumeration::Basic;
    const ALCchar *names = alcGetString(nullptr, (ALenum)type);
    while(names && *names)
    {
        list.emplace_back(names);
        names += strlen(names)+1;
    }
    return list;
}

String ALDeviceManager::defaultDeviceName(DefaultDeviceType type) const
{
    if(type == DefaultDeviceType::Complete && !alcIsExtensionPresent(0, "ALC_ENUMERATE_ALL_EXT"))
        type = DefaultDeviceType::Basic;
    const ALCchar *name = alcGetString(nullptr, (ALenum)type);
    return name ? String(name) : String();
}


Device ALDeviceManager::openPlayback(const String &name)
{
    ALCdevice *dev = alcOpenDevice(name.c_str());
    if(!dev)
    {
        if(name.empty())
            throw std::runtime_error("Failed to open default device");
        throw std::runtime_error("Failed to open device \""+name+"\"");
    }
    mDevices.emplace_back(MakeUnique<ALDevice>(dev));
    return Device(mDevices.back().get());
}

void ALDeviceManager::removeDevice(ALDevice *dev)
{
    auto iter = std::find_if(mDevices.begin(), mDevices.end(),
        [dev](const UniquePtr<ALDevice> &entry) -> bool
        { return entry.get() == dev; }
    );
    if(iter != mDevices.end()) mDevices.erase(iter);
}

}
