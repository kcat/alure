
#include "device.h"

#include <string.h>

#include <stdexcept>

#include "alc.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure
{

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
