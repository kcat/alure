
#include "device.h"

#include <string.h>

#include <stdexcept>
#include <algorithm>

#include "alc.h"

#include "context.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure
{

void ALDevice::removeContext(ALContext *ctx)
{
    std::vector<ALContext*>::iterator iter;
    iter = std::find(mContexts.begin(), mContexts.end(), ctx);
    if(iter != mContexts.end())
        mContexts.erase(iter);
}

std::string ALDevice::getName(PlaybackDeviceName type)
{
    if(type == PlaybackDevName_Complete && !alcIsExtensionPresent(mDevice, "ALC_ENUMERATE_ALL_EXT"))
        type = PlaybackDevName_Basic;
    alcGetError(mDevice);
    const ALCchar *name = alcGetString(mDevice, type);
    if(alcGetError(mDevice) != ALC_NO_ERROR || !name)
        name = alcGetString(mDevice, PlaybackDevName_Basic);
    return std::string(name);
}

bool ALDevice::queryExtension(const char *extname)
{
    return alcIsExtensionPresent(mDevice, extname);
}

Context *ALDevice::createContext(ALCint *attribs)
{
    ALCcontext *ctx = alcCreateContext(mDevice, attribs);
    if(!ctx) throw std::runtime_error("Failed to create context");

    ALContext *ret = new ALContext(ctx, this);
    mContexts.push_back(ret);
    return ret;
}

void ALDevice::close()
{
    if(!mContexts.empty())
        throw std::runtime_error("Trying to close device with contexts");

    if(alcCloseDevice(mDevice) == ALC_FALSE)
        throw std::runtime_error("Failed to close device");
    mDevice = 0;

    delete this;
}

}
