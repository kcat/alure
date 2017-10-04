
#include "config.h"

#include "device.h"

#include <string.h>

#include <stdexcept>
#include <algorithm>

#include "alc.h"
#include "alext.h"

#include "devicemanager.h"
#include "context.h"
#include "buffer.h"


namespace alure
{

template<typename T>
static inline void LoadALCFunc(ALCdevice *device, T **func, const char *name)
{ *func = reinterpret_cast<T*>(alcGetProcAddress(device, name)); }


static void LoadPauseDevice(ALDevice *device)
{
    LoadALCFunc(device->getDevice(), &device->alcDevicePauseSOFT, "alcDevicePauseSOFT");
    LoadALCFunc(device->getDevice(), &device->alcDeviceResumeSOFT, "alcDeviceResumeSOFT");
}

static void LoadHrtf(ALDevice *device)
{
    LoadALCFunc(device->getDevice(), &device->alcGetStringiSOFT, "alcGetStringiSOFT");
    LoadALCFunc(device->getDevice(), &device->alcResetDeviceSOFT, "alcResetDeviceSOFT");
}

static void LoadNothing(ALDevice*) { }

static const struct {
    enum ALCExtension extension;
    const char name[32];
    void (&loader)(ALDevice*);
} ALCExtensionList[] = {
    { EXT_thread_local_context, "ALC_EXT_thread_local_context", LoadNothing },
    { SOFT_device_pause, "ALC_SOFT_pause_device", LoadPauseDevice },
    { SOFT_HRTF, "ALC_SOFT_HRTF", LoadHrtf },
};


void ALDevice::setupExts()
{
    std::fill(std::begin(mHasExt), std::end(mHasExt), false);
    for(const auto &entry : ALCExtensionList)
    {
        mHasExt[entry.extension] = alcIsExtensionPresent(mDevice, entry.name);
        if(mHasExt[entry.extension]) entry.loader(this);
    }
}


ALDevice::ALDevice(ALCdevice* device)
  : mDevice(device), alcDevicePauseSOFT(nullptr), alcDeviceResumeSOFT(nullptr)
{
    setupExts();
}

ALDevice::~ALDevice()
{
}


void ALDevice::removeContext(ALContext *ctx)
{
    auto iter = std::find_if(mContexts.begin(), mContexts.end(),
        [ctx](const UniquePtr<ALContext> &entry) -> bool
        { return entry.get() == ctx; }
    );
    if(iter != mContexts.end()) mContexts.erase(iter);
}


String ALDevice::getName(PlaybackDeviceName type) const
{
    if(type == PlaybackDeviceName::Complete && !alcIsExtensionPresent(mDevice, "ALC_ENUMERATE_ALL_EXT"))
        type = PlaybackDeviceName::Basic;
    alcGetError(mDevice);
    const ALCchar *name = alcGetString(mDevice, (ALenum)type);
    if(alcGetError(mDevice) != ALC_NO_ERROR || !name)
        name = alcGetString(mDevice, (ALenum)PlaybackDeviceName::Basic);
    return name ? String(name) : String();
}

bool ALDevice::queryExtension(const String &name) const
{
    return alcIsExtensionPresent(mDevice, name.c_str());
}

ALCuint ALDevice::getALCVersion() const
{
    ALCint major=-1, minor=-1;
    alcGetIntegerv(mDevice, ALC_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(mDevice, ALC_MINOR_VERSION, 1, &minor);
    if(major < 0 || minor < 0)
        throw std::runtime_error("ALC version error");
    return MakeVersion(
        (ALCushort)std::min<ALCint>(major, std::numeric_limits<ALCushort>::max()),
        (ALCushort)std::min<ALCint>(minor, std::numeric_limits<ALCushort>::max())
    );
}

ALCuint ALDevice::getEFXVersion() const
{
    if(!alcIsExtensionPresent(mDevice, "ALC_EXT_EFX"))
        return 0;

    ALCint major=-1, minor=-1;
    alcGetIntegerv(mDevice, ALC_EFX_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(mDevice, ALC_EFX_MINOR_VERSION, 1, &minor);
    if(major < 0 || minor < 0)
        throw std::runtime_error("EFX version error");
    return MakeVersion(
        (ALCushort)std::min<ALCint>(major, std::numeric_limits<ALCushort>::max()),
        (ALCushort)std::min<ALCint>(minor, std::numeric_limits<ALCushort>::max())
    );
}

ALCuint ALDevice::getFrequency() const
{
    ALCint freq = -1;
    alcGetIntegerv(mDevice, ALC_FREQUENCY, 1, &freq);
    if(freq < 0)
        throw std::runtime_error("Frequency error");
    return freq;
}

ALCuint ALDevice::getMaxAuxiliarySends() const
{
    if(!alcIsExtensionPresent(mDevice, "ALC_EXT_EFX"))
        return 0;

    ALCint sends=-1;
    alcGetIntegerv(mDevice, ALC_MAX_AUXILIARY_SENDS, 1, &sends);
    if(sends < 0)
        throw std::runtime_error("Max auxiliary sends error");
    return sends;
}


Vector<String> ALDevice::enumerateHRTFNames() const
{
    if(!hasExtension(SOFT_HRTF))
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

bool ALDevice::isHRTFEnabled() const
{
    if(!hasExtension(SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");

    ALCint hrtf_state = -1;
    alcGetIntegerv(mDevice, ALC_HRTF_SOFT, 1, &hrtf_state);
    if(hrtf_state == -1)
        throw std::runtime_error("HRTF state error");
    return hrtf_state != ALC_FALSE;
}

String ALDevice::getCurrentHRTF() const
{
    if(!hasExtension(SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");
    return String(alcGetString(mDevice, ALC_HRTF_SPECIFIER_SOFT));
}

void ALDevice::reset(ArrayView<AttributePair> attributes)
{
    if(!hasExtension(SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");
    auto do_reset = [this, &attributes]() -> ALCboolean
    {
        if(attributes.empty())
        {
            /* No explicit attributes. */
            return alcResetDeviceSOFT(mDevice, nullptr);
        }
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
            attrs.reserve(attributes.size());
            std::copy(attributes.begin(), attributes.end(), std::back_inserter(attrs));
            attrs.push_back({0, 0});
            return alcResetDeviceSOFT(mDevice, &std::get<0>(attrs.front()));
        }
        return alcResetDeviceSOFT(mDevice, &std::get<0>(attributes.front()));
    };
    if(!do_reset())
        throw std::runtime_error("Device reset error");
}


Context ALDevice::createContext(ArrayView<AttributePair> attributes)
{
    ALCcontext *ctx = [this, &attributes]() -> ALCcontext*
    {
        if(attributes.empty())
        {
            /* No explicit attributes. */
            return alcCreateContext(mDevice, nullptr);
        }
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
            attrs.reserve(attributes.size());
            std::copy(attributes.begin(), attributes.end(), std::back_inserter(attrs));
            attrs.push_back({0, 0});
            return alcCreateContext(mDevice, &std::get<0>(attrs.front()));
        }
        return alcCreateContext(mDevice, &std::get<0>(attributes.front()));
    }();
    if(!ctx) throw std::runtime_error("Failed to create context");

    mContexts.emplace_back(MakeUnique<ALContext>(ctx, this));
    return Context(mContexts.back().get());
}


void ALDevice::pauseDSP()
{
    if(!hasExtension(SOFT_device_pause))
       throw std::runtime_error("ALC_SOFT_pause_device not supported");
    alcDevicePauseSOFT(mDevice);
}

void ALDevice::resumeDSP()
{
    if(hasExtension(SOFT_device_pause))
        alcDeviceResumeSOFT(mDevice);
}


void ALDevice::close()
{
    if(!mContexts.empty())
        throw std::runtime_error("Trying to close device with contexts");

    if(alcCloseDevice(mDevice) == ALC_FALSE)
        throw std::runtime_error("Failed to close device");
    mDevice = 0;

    ALDeviceManager::get().removeDevice(this);
}


DECL_THUNK1(String, Device, getName, const, PlaybackDeviceName)
DECL_THUNK1(bool, Device, queryExtension, const, const String&)
DECL_THUNK0(ALCuint, Device, getALCVersion, const)
DECL_THUNK0(ALCuint, Device, getEFXVersion, const)
DECL_THUNK0(ALCuint, Device, getFrequency, const)
DECL_THUNK0(ALCuint, Device, getMaxAuxiliarySends, const)
DECL_THUNK0(Vector<String>, Device, enumerateHRTFNames, const)
DECL_THUNK0(bool, Device, isHRTFEnabled, const)
DECL_THUNK0(String, Device, getCurrentHRTF, const)
DECL_THUNK1(void, Device, reset,, ArrayView<AttributePair>)
DECL_THUNK1(Context, Device, createContext,, ArrayView<AttributePair>)
DECL_THUNK0(void, Device, pauseDSP,)
DECL_THUNK0(void, Device, resumeDSP,)
void Device::close()
{
    pImpl->close();
    pImpl = nullptr;
}

}
