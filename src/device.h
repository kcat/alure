#ifndef DEVICE_H
#define DEVICE_H

#include "main.h"

#include <map>
#include <mutex>

#include "alc.h"
#include "alext.h"


namespace alure {

class ALContext;
class ALBuffer;


enum ALCExtension {
    EXT_thread_local_context,
    SOFT_device_pause,
    SOFT_HRTF,

    ALC_EXTENSION_MAX
};

class ALDevice {
    ALCdevice *mDevice;

    Vector<UniquePtr<ALContext>> mContexts;

    bool mHasExt[ALC_EXTENSION_MAX];

    std::once_flag mSetExts;
    void setupExts();

public:
    ALDevice(ALCdevice *device);
    virtual ~ALDevice();

    ALCdevice *getDevice() const { return mDevice; }

    bool hasExtension(ALCExtension ext) const { return mHasExt[ext]; }

    LPALCDEVICEPAUSESOFT alcDevicePauseSOFT;
    LPALCDEVICERESUMESOFT alcDeviceResumeSOFT;

    LPALCGETSTRINGISOFT alcGetStringiSOFT;
    LPALCRESETDEVICESOFT alcResetDeviceSOFT;

    void removeContext(ALContext *ctx);

    String getName(PlaybackDeviceName type) const;
    bool queryExtension(const String &name) const;

    ALCuint getALCVersion() const;
    ALCuint getEFXVersion() const;

    ALCuint getFrequency() const;

    ALCuint getMaxAuxiliarySends() const;

    Vector<String> enumerateHRTFNames() const;
    bool isHRTFEnabled() const;
    String getCurrentHRTF() const;
    void reset(const Vector<AttributePair> &attributes);

    Context createContext(const Vector<AttributePair> &attributes);

    void pauseDSP();
    void resumeDSP();

    void close();
};

} // namespace alure

#endif /* DEVICE_H */
