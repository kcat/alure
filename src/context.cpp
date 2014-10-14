
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
    if(mRefs.load() != 0)
        throw std::runtime_error("Context is in use");

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
    if(alcMakeContextCurrent(context ? context->getContext() : 0) == ALC_FALSE)
        return false;
    if(context)
        context->addRef();
    if(sCurrentCtx)
        sCurrentCtx->decRef();
    sCurrentCtx = context;
    return true;
}


bool Context::MakeCurrent(Context *context)
{
    ALContext *ctx = 0;
    if(context)
    {
        ctx = dynamic_cast<ALContext*>(context);
        if(!ctx) throw std::runtime_error("Invalid context pointer");
    }
    return ALContext::MakeCurrent(ctx);
}

Context *Context::GetCurrent()
{
    return ALContext::GetCurrent();
}

}
