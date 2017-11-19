
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

DeviceManager DeviceManager::get()
{ return DeviceManager(&DeviceManagerImpl::get()); }
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


bool DeviceManager::queryExtension(const String &name) const
{ return pImpl->queryExtension(name.c_str()); }
DECL_THUNK1(bool, DeviceManager, queryExtension, const, const char*)
bool DeviceManagerImpl::queryExtension(const char *name) const
{
    return alcIsExtensionPresent(nullptr, name);
}

DECL_THUNK1(Vector<String>, DeviceManager, enumerate, const, DeviceEnumeration)
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

DECL_THUNK1(String, DeviceManager, defaultDeviceName, const, DefaultDeviceType)
String DeviceManagerImpl::defaultDeviceName(DefaultDeviceType type) const
{
    if(type == DefaultDeviceType::Full && !alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"))
        type = DefaultDeviceType::Basic;
    const ALCchar *name = alcGetString(nullptr, (ALenum)type);
    return name ? String(name) : String();
}


Device DeviceManager::openPlayback(const String &name)
{ return pImpl->openPlayback(name.c_str()); }
DECL_THUNK1(Device, DeviceManager, openPlayback,, const char*)
Device DeviceManagerImpl::openPlayback(const char *name)
{
    ALCdevice *dev = alcOpenDevice(name);
    if(dev)
    {
        try {
            mDevices.emplace_back(MakeUnique<DeviceImpl>(dev));
            return Device(mDevices.back().get());
        }
        catch(...) {
            alcCloseDevice(dev);
            throw;
        }
    }
    throw std::runtime_error("Device open failed");
}

Device DeviceManager::openPlayback(const String &name, const std::nothrow_t &nt) noexcept
{ return pImpl->openPlayback(name.c_str(), nt); }
Device DeviceManager::openPlayback(const std::nothrow_t&) noexcept
{ return pImpl->openPlayback(nullptr, std::nothrow); }
DECL_THUNK2(Device, DeviceManager, openPlayback, noexcept, const char*, const std::nothrow_t&)
Device DeviceManagerImpl::openPlayback(const char *name, const std::nothrow_t&) noexcept
{
    try {
        return openPlayback(name);
    }
    catch(...) {
    }
    return Device();
}

void DeviceManagerImpl::removeDevice(DeviceImpl *dev)
{
    auto iter = std::find_if(mDevices.begin(), mDevices.end(),
        [dev](const UniquePtr<DeviceImpl> &entry) -> bool
        { return entry.get() == dev; }
    );
    if(iter != mDevices.end()) mDevices.erase(iter);
}

}
