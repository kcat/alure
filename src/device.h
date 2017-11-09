#ifndef DEVICE_H
#define DEVICE_H

#include "main.h"

#include <mutex>


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
