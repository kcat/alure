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

class ALDevice : public Device {
    ALCdevice *mDevice;

    Vector<ALContext*> mContexts;

    bool mHasExt[ALC_EXTENSION_MAX];

    std::once_flag mSetExts;
    void setupExts();

    virtual ~ALDevice();
public:
    ALDevice(ALCdevice *device);

    ALCdevice *getDevice() const { return mDevice; }

    bool hasExtension(ALCExtension ext) const { return mHasExt[ext]; }

    LPALCDEVICEPAUSESOFT alcDevicePauseSOFT;
    LPALCDEVICERESUMESOFT alcDeviceResumeSOFT;

    LPALCGETSTRINGISOFT alcGetStringiSOFT;
    LPALCRESETDEVICESOFT alcResetDeviceSOFT;

    void removeContext(ALContext *ctx);

    String getName(PlaybackDeviceName type) const override final;
    bool queryExtension(const String &name) const override final;

    ALCuint getALCVersion() const override final;
    ALCuint getEFXVersion() const override final;

    ALCuint getFrequency() const override final;

    ALCuint getMaxAuxiliarySends() const override final;

    Vector<String> enumerateHRTFNames() const override final;
    bool isHRTFEnabled() const override final;
    String getCurrentHRTF() const override final;
    void reset(const ALCint *attributes) override final;

    Context *createContext(const ALCint *attribs=0) override final;

    void pauseDSP() override final;
    void resumeDSP() override final;

    void close() override final;
};

} // namespace alure

#endif /* DEVICE_H */
