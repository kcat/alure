
#include "config.h"

#include "devicemanager.h"
#include "device.h"

#include <string.h>

#include <algorithm>
#include <stdexcept>
#include <iostream>

#include "alc.h"
#include "al.h"

namespace alure
{

template<typename T>
static inline void GetDeviceProc(T **func, ALCdevice *device, const char *name)
{ *func = reinterpret_cast<T*>(alcGetProcAddress(device, name)); }


ALCboolean (ALC_APIENTRY*DeviceManagerImpl::SetThreadContext)(ALCcontext*);

DeviceManagerImpl &DeviceManagerImpl::get()
{
    static DeviceManagerImpl singleton;
    return singleton;
}

DeviceManagerImpl::DeviceManagerImpl()
{
    if(alcIsExtensionPresent(0, "ALC_EXT_thread_local_context"))
        GetDeviceProc(&SetThreadContext, 0, "alcSetThreadContext");
}

DeviceManagerImpl::~DeviceManagerImpl()
{
}


bool DeviceManagerImpl::queryExtension(StringView name) const
{
    return alcIsExtensionPresent(nullptr, String(name).c_str());
}

Vector<String> DeviceManagerImpl::enumerate(DeviceEnumeration type) const
{
    Vector<String> list;
    if(type == DeviceEnumeration::Full && !alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"))
        type = DeviceEnumeration::Basic;
    const ALCchar *names = alcGetString(nullptr, (ALenum)type);
    while(names && *names)
    {
        list.emplace_back(names);
        names += strlen(names)+1;
    }
    return list;
}

String DeviceManagerImpl::defaultDeviceName(DefaultDeviceType type) const
{
    if(type == DefaultDeviceType::Full && !alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"))
        type = DefaultDeviceType::Basic;
    const ALCchar *name = alcGetString(nullptr, (ALenum)type);
    return name ? String(name) : String();
}


Device DeviceManagerImpl::openPlayback(StringView name)
{
    ALCdevice *dev = alcOpenDevice(String(name).c_str());
    if(!dev)
    {
        if(name.empty())
            throw std::runtime_error("Failed to open default device");
        throw std::runtime_error("Failed to open device \""+name+"\"");
    }
    mDevices.emplace_back(MakeUnique<DeviceImpl>(dev));
    return Device(mDevices.back().get());
}

Device DeviceManagerImpl::openPlayback(StringView name, const std::nothrow_t&)
{
    ALCdevice *dev = alcOpenDevice(String(name).c_str());
    if(!dev) return Device();
    mDevices.emplace_back(MakeUnique<DeviceImpl>(dev));
    return Device(mDevices.back().get());
}

void DeviceManagerImpl::removeDevice(DeviceImpl *dev)
{
    auto iter = std::find_if(mDevices.begin(), mDevices.end(),
        [dev](const UniquePtr<DeviceImpl> &entry) -> bool
        { return entry.get() == dev; }
    );
    if(iter != mDevices.end()) mDevices.erase(iter);
}


DeviceManager DeviceManager::get()
{ return DeviceManager(&DeviceManagerImpl::get()); }
DECL_THUNK1(bool, DeviceManager, queryExtension, const, StringView)
DECL_THUNK1(Vector<String>, DeviceManager, enumerate, const, DeviceEnumeration)
DECL_THUNK1(String, DeviceManager, defaultDeviceName, const, DefaultDeviceType)
DECL_THUNK1(Device, DeviceManager, openPlayback,, StringView)
DECL_THUNK2(Device, DeviceManager, openPlayback,, StringView, const std::nothrow_t&)
Device DeviceManager::openPlayback(const std::nothrow_t&)
{ return openPlayback(StringView(), std::nothrow); }

}
