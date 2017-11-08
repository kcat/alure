#ifndef DEVICE_H
#define DEVICE_H

#include "main.h"

#include <mutex>

#include "alc.h"
#include "alext.h"


extern "C" {
#ifndef ALC_SOFT_pause_device
#define ALC_SOFT_pause_device 1
typedef void (ALC_APIENTRY*LPALCDEVICEPAUSESOFT)(ALCdevice *device);
typedef void (ALC_APIENTRY*LPALCDEVICERESUMESOFT)(ALCdevice *device);
#endif

#ifndef ALC_SOFT_HRTF
#define ALC_SOFT_HRTF 1
#define ALC_HRTF_SOFT                            0x1992
#define ALC_DONT_CARE_SOFT                       0x0002
#define ALC_HRTF_STATUS_SOFT                     0x1993
#define ALC_HRTF_DISABLED_SOFT                   0x0000
#define ALC_HRTF_ENABLED_SOFT                    0x0001
#define ALC_HRTF_DENIED_SOFT                     0x0002
#define ALC_HRTF_REQUIRED_SOFT                   0x0003
#define ALC_HRTF_HEADPHONES_DETECTED_SOFT        0x0004
#define ALC_HRTF_UNSUPPORTED_FORMAT_SOFT         0x0005
#define ALC_NUM_HRTF_SPECIFIERS_SOFT             0x1994
#define ALC_HRTF_SPECIFIER_SOFT                  0x1995
#define ALC_HRTF_ID_SOFT                         0x1996
typedef const ALCchar* (ALC_APIENTRY*LPALCGETSTRINGISOFT)(ALCdevice *device, ALCenum paramName, ALCsizei index);
typedef ALCboolean (ALC_APIENTRY*LPALCRESETDEVICESOFT)(ALCdevice *device, const ALCint *attribs);
#endif

} // extern "C"

namespace alure {


enum class ALC {
    ENUMERATE_ALL_EXT,
    EXT_EFX,
    EXT_thread_local_context,
    SOFT_device_pause,
    SOFT_HRTF,

    EXTENSION_MAX
};

class DeviceImpl {
    ALCdevice *mDevice;

    Vector<UniquePtr<ContextImpl>> mContexts;

    Bitfield<static_cast<size_t>(ALC::EXTENSION_MAX)> mHasExt;

    std::once_flag mSetExts;
    void setupExts();

public:
    DeviceImpl(ALCdevice *device);
    ~DeviceImpl();

    ALCdevice *getALCdevice() const { return mDevice; }

    bool hasExtension(ALC ext) const { return mHasExt[static_cast<size_t>(ext)]; }

    LPALCDEVICEPAUSESOFT alcDevicePauseSOFT;
    LPALCDEVICERESUMESOFT alcDeviceResumeSOFT;

    LPALCGETSTRINGISOFT alcGetStringiSOFT;
    LPALCRESETDEVICESOFT alcResetDeviceSOFT;

    void removeContext(ContextImpl *ctx);

    String getName(PlaybackName type) const;
    bool queryExtension(const char *name) const;

    Version getALCVersion() const;
    Version getEFXVersion() const;

    ALCuint getFrequency() const;

    ALCuint getMaxAuxiliarySends() const;

    Vector<String> enumerateHRTFNames() const;
    bool isHRTFEnabled() const;
    String getCurrentHRTF() const;
    void reset(ArrayView<AttributePair> attributes);

    Context createContext(ArrayView<AttributePair> attributes, bool dothrow);

    void pauseDSP();
    void resumeDSP();

    void close();
};

} // namespace alure

#endif /* DEVICE_H */
