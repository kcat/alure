
#include "config.h"

#include "device.h"

#include <string.h>

#include <stdexcept>
#include <algorithm>

#include "devicemanager.h"
#include "context.h"
#include "buffer.h"


namespace alure
{

template<typename T>
static inline void LoadALCFunc(ALCdevice *device, T **func, const char *name)
{ *func = reinterpret_cast<T*>(alcGetProcAddress(device, name)); }


static void LoadPauseDevice(DeviceImpl *device)
{
    LoadALCFunc(device->getALCdevice(), &device->alcDevicePauseSOFT, "alcDevicePauseSOFT");
    LoadALCFunc(device->getALCdevice(), &device->alcDeviceResumeSOFT, "alcDeviceResumeSOFT");
}

static void LoadHrtf(DeviceImpl *device)
{
    LoadALCFunc(device->getALCdevice(), &device->alcGetStringiSOFT, "alcGetStringiSOFT");
    LoadALCFunc(device->getALCdevice(), &device->alcResetDeviceSOFT, "alcResetDeviceSOFT");
}

static void LoadNothing(DeviceImpl*) { }

static const struct {
    enum ALC extension;
    const char name[32];
    void (&loader)(DeviceImpl*);
} ALCExtensionList[] = {
    { ALC::ENUMERATE_ALL_EXT, "ALC_ENUMERATE_ALL_EXT", LoadNothing },
    { ALC::EXT_EFX, "ALC_EXT_EFX", LoadNothing },
    { ALC::EXT_thread_local_context, "ALC_EXT_thread_local_context", LoadNothing },
    { ALC::SOFT_device_pause, "ALC_SOFT_pause_device", LoadPauseDevice },
    { ALC::SOFT_HRTF, "ALC_SOFT_HRTF", LoadHrtf },
};


void DeviceImpl::setupExts()
{
    mHasExt.clear();
    for(const auto &entry : ALCExtensionList)
    {
        if(!alcIsExtensionPresent(mDevice, entry.name))
            continue;
        mHasExt.set(static_cast<size_t>(entry.extension));
        entry.loader(this);
    }
}


DeviceImpl::DeviceImpl(ALCdevice* device)
  : mDevice(device), alcDevicePauseSOFT(nullptr), alcDeviceResumeSOFT(nullptr)
{
    setupExts();
}

DeviceImpl::~DeviceImpl()
{
}


void DeviceImpl::removeContext(ContextImpl *ctx)
{
    auto iter = std::find_if(mContexts.begin(), mContexts.end(),
        [ctx](const UniquePtr<ContextImpl> &entry) -> bool
        { return entry.get() == ctx; }
    );
    if(iter != mContexts.end()) mContexts.erase(iter);
}


DECL_THUNK1(String, Device, getName, const, PlaybackName)
String DeviceImpl::getName(PlaybackName type) const
{
    if(type == PlaybackName::Full && !hasExtension(ALC::ENUMERATE_ALL_EXT))
        type = PlaybackName::Basic;
    alcGetError(mDevice);
    const ALCchar *name = alcGetString(mDevice, (ALenum)type);
    if(alcGetError(mDevice) != ALC_NO_ERROR || !name)
        name = alcGetString(mDevice, (ALenum)PlaybackName::Basic);
    return name ? String(name) : String();
}

bool Device::queryExtension(const String &name) const
{ return pImpl->queryExtension(name.c_str()); }
DECL_THUNK1(bool, Device, queryExtension, const, const char*)
bool DeviceImpl::queryExtension(const char *name) const
{
    return alcIsExtensionPresent(mDevice, name);
}

DECL_THUNK0(Version, Device, getALCVersion, const)
Version DeviceImpl::getALCVersion() const
{
    ALCint major=-1, minor=-1;
    alcGetIntegerv(mDevice, ALC_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(mDevice, ALC_MINOR_VERSION, 1, &minor);
    if(major < 0 || minor < 0)
        throw std::runtime_error("ALC version error");
    return Version{ (ALCuint)major, (ALCuint)minor };
}

DECL_THUNK0(Version, Device, getEFXVersion, const)
Version DeviceImpl::getEFXVersion() const
{
    if(!hasExtension(ALC::EXT_EFX))
        return Version{ 0u, 0u };

    ALCint major=-1, minor=-1;
    alcGetIntegerv(mDevice, ALC_EFX_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(mDevice, ALC_EFX_MINOR_VERSION, 1, &minor);
    if(major < 0 || minor < 0)
        throw std::runtime_error("EFX version error");
    return Version{ (ALCuint)major, (ALCuint)minor };
}

DECL_THUNK0(ALCuint, Device, getFrequency, const)
ALCuint DeviceImpl::getFrequency() const
{
    ALCint freq = -1;
    alcGetIntegerv(mDevice, ALC_FREQUENCY, 1, &freq);
    if(freq < 0)
        throw std::runtime_error("Frequency error");
    return freq;
}

DECL_THUNK0(ALCuint, Device, getMaxAuxiliarySends, const)
ALCuint DeviceImpl::getMaxAuxiliarySends() const
{
    if(!hasExtension(ALC::EXT_EFX))
        return 0;

    ALCint sends=-1;
    alcGetIntegerv(mDevice, ALC_MAX_AUXILIARY_SENDS, 1, &sends);
    if(sends < 0)
        throw std::runtime_error("Max auxiliary sends error");
    return sends;
}


DECL_THUNK0(Vector<String>, Device, enumerateHRTFNames, const)
Vector<String> DeviceImpl::enumerateHRTFNames() const
{
    if(!hasExtension(ALC::SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");

    ALCint num_hrtfs = -1;
    alcGetIntegerv(mDevice, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &num_hrtfs);
    if(num_hrtfs < 0)
        throw std::runtime_error("HRTF specifier count error");

    Vector<String> hrtfs;
    hrtfs.reserve(num_hrtfs);
    for(int i = 0;i < num_hrtfs;++i)
        hrtfs.emplace_back(alcGetStringiSOFT(mDevice, ALC_HRTF_SPECIFIER_SOFT, i));
    return hrtfs;
}

DECL_THUNK0(bool, Device, isHRTFEnabled, const)
bool DeviceImpl::isHRTFEnabled() const
{
    if(!hasExtension(ALC::SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");

    ALCint hrtf_state = -1;
    alcGetIntegerv(mDevice, ALC_HRTF_SOFT, 1, &hrtf_state);
    if(hrtf_state == -1)
        throw std::runtime_error("HRTF state error");
    return hrtf_state != ALC_FALSE;
}

DECL_THUNK0(String, Device, getCurrentHRTF, const)
String DeviceImpl::getCurrentHRTF() const
{
    if(!hasExtension(ALC::SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");
    return String(alcGetString(mDevice, ALC_HRTF_SPECIFIER_SOFT));
}

DECL_THUNK1(void, Device, reset,, ArrayView<AttributePair>)
void DeviceImpl::reset(ArrayView<AttributePair> attributes)
{
    if(!hasExtension(ALC::SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");
    bool success = false;
    if(attributes.end()) /* No explicit attributes. */
        success = alcResetDeviceSOFT(mDevice, nullptr);
    else
    {
        auto attr_end = std::find_if(attributes.begin(), attributes.end(),
            [](const AttributePair &attr) -> bool
            { return std::get<0>(attr) == 0; }
        );
        if(attr_end == attributes.end())
        {
            /* Attribute list was not properly terminated. Copy the attribute
             * list and add the 0 sentinel.
             */
            Vector<AttributePair> attrs;
            attrs.reserve(attributes.size() + 1);
            std::copy(attributes.begin(), attributes.end(), std::back_inserter(attrs));
            attrs.push_back(AttributesEnd());
            success = alcResetDeviceSOFT(mDevice, &std::get<0>(attrs.front()));
        }
        else
            success = alcResetDeviceSOFT(mDevice, &std::get<0>(attributes.front()));
    };
    if(!success)
        throw std::runtime_error("Device reset error");
}


DECL_THUNK1(Context, Device, createContext,, ArrayView<AttributePair>)
Context DeviceImpl::createContext(ArrayView<AttributePair> attributes)
{
    ALCcontext *ctx = nullptr;
    if(attributes.empty()) /* No explicit attributes. */
        ctx = alcCreateContext(mDevice, nullptr);
    else
    {
        auto attr_end = std::find_if(attributes.begin(), attributes.end(),
            [](const AttributePair &attr) -> bool
            { return std::get<0>(attr) == 0; }
        );
        if(attr_end == attributes.end())
        {
            /* Attribute list was not properly terminated. Copy the attribute
             * list and add the 0 sentinel.
             */
            Vector<AttributePair> attrs;
            attrs.reserve(attributes.size() + 1);
            std::copy(attributes.begin(), attributes.end(), std::back_inserter(attrs));
            attrs.push_back(AttributesEnd());
            ctx = alcCreateContext(mDevice, &std::get<0>(attrs.front()));
        }
        else
            ctx = alcCreateContext(mDevice, &std::get<0>(attributes.front()));
    }
    if(ctx)
    {
        try {
            mContexts.emplace_back(MakeUnique<ContextImpl>(ctx, this));
            return Context(mContexts.back().get());
        }
        catch(...) {
            alcDestroyContext(ctx);
            throw;
        }
    }
    throw std::runtime_error("Failed to create context");
}
Context Device::createContext(ArrayView<AttributePair> attrs, const std::nothrow_t&) noexcept
{
    try {
        return pImpl->createContext(attrs);
    }
    catch(...) {
    }
    return Context();
}
Context Device::createContext(const std::nothrow_t&) noexcept
{
    try {
        return pImpl->createContext({});
    }
    catch(...) {
    }
    return Context();
}


DECL_THUNK0(void, Device, pauseDSP,)
void DeviceImpl::pauseDSP()
{
    if(!hasExtension(ALC::SOFT_device_pause))
       throw std::runtime_error("ALC_SOFT_pause_device not supported");
    alcDevicePauseSOFT(mDevice);
}

DECL_THUNK0(void, Device, resumeDSP,)
void DeviceImpl::resumeDSP()
{
    if(hasExtension(ALC::SOFT_device_pause))
        alcDeviceResumeSOFT(mDevice);
}


void Device::close()
{
    DeviceImpl *i = pImpl;
    pImpl = nullptr;
    i->close();
}
void DeviceImpl::close()
{
    if(!mContexts.empty())
        throw std::runtime_error("Trying to close device with contexts");

    if(alcCloseDevice(mDevice) == ALC_FALSE)
        throw std::runtime_error("Failed to close device");
    mDevice = 0;

    DeviceManagerImpl::get().removeDevice(this);
}

}
