#ifndef DEVICE_H
#define DEVICE_H

#include "alure2.h"

#include <map>

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALContext;
class ALBuffer;

class ALDevice : public Device {
public:
    typedef std::map<std::string,ALBuffer*> BufferMap;

private:
    ALCdevice *mDevice;

    std::vector<ALContext*> mContexts;
    BufferMap mBuffers;

    virtual ~ALDevice() { }
public:
    ALDevice(ALCdevice *device) : mDevice(device) { }

    ALCdevice *getDevice() const { return mDevice; }

    void removeContext(ALContext *ctx);

    Buffer *getBuffer(const std::string &name);
    Buffer *addBuffer(const std::string &name, ALBuffer *buffer);
    void removeBuffer(const std::string &name);
    void removeBuffer(Buffer *buffer);

    virtual std::string getName(PlaybackDeviceType type) final;
    virtual bool queryExtension(const char *extname) final;

    virtual ALCuint getALCVersion() final;
    virtual ALCuint getEFXVersion() final;

    virtual ALCuint getFrequency() final;

    virtual ALCuint getMaxAuxiliarySends() final;

    virtual Context *createContext(ALCint *attribs=0) final;

    virtual void close() final;
};

} // namespace alure

#endif /* DEVICE_H */
