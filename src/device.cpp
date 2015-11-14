
#include "config.h"

#include "device.h"

#include <string.h>

#include <stdexcept>
#include <algorithm>

#include "alc.h"
#include "alext.h"

#include "context.h"
#include "buffer.h"


namespace alure
{

template<typename T, size_t N>
static inline size_t countof(const T(&)[N])
{ return N; }


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
    void (*loader)(ALDevice*);
} ALCExtensionList[] = {
    { EXT_thread_local_context, "ALC_EXT_thread_local_context", LoadNothing },
    { SOFT_device_pause, "ALC_SOFT_pause_device", LoadPauseDevice },
    { SOFT_HRTF, "ALC_SOFT_HRTF", LoadHrtf },
};


void ALDevice::setupExts()
{
    for(size_t i = 0;i < countof(ALCExtensionList);i++)
    {
        mHasExt[ALCExtensionList[i].extension] = alcIsExtensionPresent(mDevice, ALCExtensionList[i].name);
        if(mHasExt[ALCExtensionList[i].extension])
            ALCExtensionList[i].loader(this);
    }
}


ALDevice::ALDevice(ALCdevice* device)
  : mDevice(device), mHasExt{false}, alcDevicePauseSOFT(nullptr), alcDeviceResumeSOFT(nullptr)
{
    setupExts();
}

void ALDevice::removeContext(ALContext *ctx)
{
    std::vector<ALContext*>::iterator iter;
    iter = std::find(mContexts.begin(), mContexts.end(), ctx);
    if(iter != mContexts.end())
        mContexts.erase(iter);
}


Buffer *ALDevice::getBuffer(const std::string &name)
{
    BufferMap::const_iterator iter = mBuffers.find(name);
    if(iter == mBuffers.end()) return 0;
    return iter->second;
}

Buffer *ALDevice::addBuffer(const std::string &name, ALBuffer *buffer)
{
    mBuffers.insert(std::make_pair(name, buffer));
    return buffer;
}

void ALDevice::removeBuffer(const std::string &name)
{
    BufferMap::iterator iter = mBuffers.find(name);
    if(iter != mBuffers.end())
    {
        ALBuffer *albuf = iter->second;
        albuf->cleanup();
        mBuffers.erase(iter);
    }
}

void ALDevice::removeBuffer(Buffer *buffer)
{
    BufferMap::iterator iter = mBuffers.begin();
    while(iter != mBuffers.end())
    {
        ALBuffer *albuf = iter->second;
        if(albuf == buffer)
        {
            albuf->cleanup();
            mBuffers.erase(iter);
            break;
        }
        ++iter;
    }
}


std::string ALDevice::getName(PlaybackDeviceType type) const
{
    if(type == PlaybackDevType_Complete && !alcIsExtensionPresent(mDevice, "ALC_ENUMERATE_ALL_EXT"))
        type = PlaybackDevType_Basic;
    alcGetError(mDevice);
    const ALCchar *name = alcGetString(mDevice, type);
    if(alcGetError(mDevice) != ALC_NO_ERROR || !name)
        name = alcGetString(mDevice, PlaybackDevType_Basic);
    return std::string(name ? name : "");
}

bool ALDevice::queryExtension(const char *extname) const
{
    return alcIsExtensionPresent(mDevice, extname);
}

ALCuint ALDevice::getALCVersion() const
{
    ALCint major=-1, minor=-1;
    alcGetIntegerv(mDevice, ALC_MAJOR_VERSION, 1, &major);
    alcGetIntegerv(mDevice, ALC_MINOR_VERSION, 1, &minor);
    if(major < 0 || minor < 0)
        throw std::runtime_error("ALC version error");
    major = std::min<ALCint>(major, std::numeric_limits<ALCushort>::max());
    minor = std::min<ALCint>(minor, std::numeric_limits<ALCushort>::max());
    return MakeVersion((ALCushort)major, (ALCushort)minor);
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
    major = std::min<ALCint>(major, std::numeric_limits<ALCushort>::max());
    minor = std::min<ALCint>(minor, std::numeric_limits<ALCushort>::max());
    return MakeVersion((ALCushort)major, (ALCushort)minor);
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
    if(sends == -1)
        throw std::runtime_error("Max auxiliary sends error");
    return sends;
}


std::vector<std::string> ALDevice::enumerateHRTFNames() const
{
    if(!hasExtension(SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");

    ALCint num_hrtfs = -1;
    alcGetIntegerv(mDevice, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &num_hrtfs);
    if(num_hrtfs == -1)
        throw std::runtime_error("HRTF specifier count error");

    std::vector<std::string> hrtfs;
    hrtfs.reserve(num_hrtfs);
    for(int i = 0;i < num_hrtfs;++i)
        hrtfs.push_back(alcGetStringiSOFT(mDevice, ALC_HRTF_SPECIFIER_SOFT, i));
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

std::string ALDevice::getCurrentHRTF() const
{
    if(!hasExtension(SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");
    return std::string(alcGetString(mDevice, ALC_HRTF_SPECIFIER_SOFT));
}

void ALDevice::reset(ALCint *attributes)
{
    if(!hasExtension(SOFT_HRTF))
        throw std::runtime_error("ALC_SOFT_HRTF not supported");
    if(!alcResetDeviceSOFT(mDevice, attributes))
        throw std::runtime_error("Device reset error");
}


Context *ALDevice::createContext(ALCint *attribs)
{
    ALCcontext *ctx = alcCreateContext(mDevice, attribs);
    if(!ctx) throw std::runtime_error("Failed to create context");

    ALContext *ret = new ALContext(ctx, this);
    mContexts.push_back(ret);
    return ret;
}


bool ALDevice::isAsyncSupported() const
{
    if(hasExtension(EXT_thread_local_context) && alcIsExtensionPresent(0, "ALC_EXT_thread_local_context"))
        return true;
    return false;
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
    if(!mBuffers.empty())
        throw std::runtime_error("Trying to close device with buffers");

    if(alcCloseDevice(mDevice) == ALC_FALSE)
        throw std::runtime_error("Failed to close device");
    mDevice = 0;

    delete this;
}

}
