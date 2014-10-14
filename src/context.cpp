
#include "context.h"

#include <stdexcept>

#include "alc.h"

#include "device.h"

namespace alure
{

ALContext *ALContext::sCurrentCtx = 0;

ALContext::~ALContext()
{
    mDevice->removeContext(this);
}

void ALContext::destroy()
{
    if(this == sCurrentCtx)
    {
        alcMakeContextCurrent(0);
        sCurrentCtx = 0;
    }
    alcDestroyContext(mContext);
    mContext = 0;
    delete this;
}

Device *ALContext::getDevice()
{
    return mDevice;
}


bool ALContext::MakeCurrent(ALContext *context)
{
    if(alcMakeContextCurrent(context->getContext()) == ALC_FALSE)
        return false;
    sCurrentCtx = context;
    return true;
}


bool Context::MakeCurrent(Context *context)
{
    ALContext *ctx = dynamic_cast<ALContext*>(context);
    if(!ctx) throw std::runtime_error("Invalid context pointer");
    return ALContext::MakeCurrent(ctx);
}

Context *Context::GetCurrent()
{
    return ALContext::GetCurrent();
}

}
