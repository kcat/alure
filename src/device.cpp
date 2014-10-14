
#include "device.h"

#include <string.h>

#include <stdexcept>

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure
{

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

void ALDevice::close()
{
    if(alcCloseDevice(mDevice) == ALC_FALSE)
        throw std::runtime_error("Failed to close device");
    mDevice = 0;

    delete this;
}

}
