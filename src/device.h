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

    virtual String getName(PlaybackDeviceName type) const final;
    virtual bool queryExtension(const char *extname) const final;

    virtual ALCuint getALCVersion() const final;
    virtual ALCuint getEFXVersion() const final;

    virtual ALCuint getFrequency() const final;

    virtual ALCuint getMaxAuxiliarySends() const final;

    virtual Vector<String> enumerateHRTFNames() const final;
    virtual bool isHRTFEnabled() const final;
    virtual String getCurrentHRTF() const final;
    virtual void reset(const ALCint *attributes) final;

    virtual Context *createContext(const ALCint *attribs=0) final;

    virtual void pauseDSP() final;
    virtual void resumeDSP() final;

    virtual void close() final;
};

} // namespace alure

#endif /* DEVICE_H */
