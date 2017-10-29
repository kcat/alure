#ifndef DEVICE_H
#define DEVICE_H

#include "main.h"

#include <mutex>

#include "alc.h"
#include "alext.h"


namespace alure {


enum ALCExtension {
    EXT_thread_local_context,
    SOFT_device_pause,
    SOFT_HRTF,

    ALC_EXTENSION_MAX
};

class DeviceImpl {
    ALCdevice *mDevice;

    Vector<UniquePtr<ContextImpl>> mContexts;

    bool mHasExt[ALC_EXTENSION_MAX];

    std::once_flag mSetExts;
    void setupExts();

public:
    DeviceImpl(ALCdevice *device);
    ~DeviceImpl();

    ALCdevice *getALCdevice() const { return mDevice; }

    bool hasExtension(ALCExtension ext) const { return mHasExt[ext]; }

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
