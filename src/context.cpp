
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <algorithm>
#include <memory>

#include "alc.h"

#ifdef HAVE_LIBSNDFILE
#include "decoders/sndfile1.h"
#endif

#include "devicemanager.h"
#include "device.h"
#include "buffer.h"
#include "source.h"

namespace alure
{

void CheckContextDevice(ALDevice *device)
{
    ALContext *thrdctx = ALContext::GetThreadCurrent();
    ALContext *globctx = ALContext::GetCurrent();
    if((!thrdctx && !globctx) || (thrdctx && device != thrdctx->getDevice()) || (!thrdctx && device != globctx->getDevice()))
        throw std::runtime_error("Called context is not current");
}

ALContext *ALContext::sCurrentCtx = 0;
#if __cplusplus >= 201103L
thread_local ALContext *ALContext::sThreadCurrentCtx;
#elif defined(_WIN32)
__declspec(thread) ALContext *ALContext::sThreadCurrentCtx;
#else
__thread ALContext *ALContext::sThreadCurrentCtx;
#endif

void ALContext::MakeCurrent(ALContext *context)
{
    if(alcMakeContextCurrent(context ? context->getContext() : 0) == ALC_FALSE)
        throw std::runtime_error("Call to alcMakeContextCurrent failed");
    if(context)
        context->addRef();
    if(sCurrentCtx)
        sCurrentCtx->decRef();
    sCurrentCtx = context;
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = 0;
}

void ALContext::MakeThreadCurrent(ALContext *context)
{
    if(!ALDeviceManager::SetThreadContext)
        throw std::runtime_error("Thread-local contexts unsupported");
    if(ALDeviceManager::SetThreadContext(context ? context->getContext() : 0) == ALC_FALSE)
        throw std::runtime_error("Call to alcSetThreadContext failed");
    if(context)
        context->addRef();
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = context;
}


ALContext::~ALContext()
{
    mDevice->removeContext(this);
}

Device *ALContext::getDevice()
{
    return mDevice;
}

void ALContext::destroy()
{
    if(mRefs.load() != 0)
        throw std::runtime_error("Context is in use");

    alcDestroyContext(mContext);
    mContext = 0;
    delete this;
}


void ALContext::startBatch()
{
    alcSuspendContext(mContext);
}

void ALContext::endBatch()
{
    alcProcessContext(mContext);
}


Decoder *ALContext::createDecoder(const std::string &name)
{
#ifdef HAVE_LIBSNDFILE
    return SndFileDecoder::openFile(name.c_str());
#endif
    throw std::runtime_error("No decoder for file");
}


Buffer *ALContext::fillBuffer(const std::string &name, Decoder *decoder)
{
    ALuint srate = decoder->getFrequency();
    SampleConfig chans = decoder->getSampleConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();

    std::vector<ALbyte> data(FramesToBytes(frames, chans, type));
    frames = decoder->read(&data[0], frames);
    if(!frames) throw std::runtime_error("No samples for buffer");
    data.resize(FramesToBytes(frames, chans, type));

    alGetError();
    ALuint bid = 0;
    try {
        alGenBuffers(1, &bid);
        alBufferData(bid, GetFormat(chans, type), &data[0], data.size(), srate);
        if(alGetError() != AL_NO_ERROR)
            throw std::runtime_error("Failed to buffer data");

        if(name.empty()) return new ALBuffer(mDevice, bid);
        return mDevice->addBuffer(name, new ALBuffer(mDevice, bid));
    }
    catch(...) {
        alDeleteBuffers(1, &bid);
        throw;
    }
}

Buffer *ALContext::getBuffer(const std::string &name)
{
    CheckContext(this);

    Buffer *buffer = mDevice->getBuffer(name);
    if(buffer) return buffer;

    std::auto_ptr<Decoder> decoder(createDecoder(name.c_str()));
    return fillBuffer(name, decoder.get());
}

Buffer *ALContext::getBuffer(Decoder *decoder)
{
    CheckContext(this);

    decoder->seek(0);
    return fillBuffer(std::string(), decoder);
}


void ALContext::removeBuffer(const std::string &name)
{
    CheckContext(this);
    mDevice->removeBuffer(name);
}

void ALContext::removeBuffer(Buffer *buffer)
{
    CheckContext(this);
    mDevice->removeBuffer(buffer);
}


ALuint ALContext::getSourceId()
{
    CheckContext(this);

    ALuint id = 0;
    if(!mSourceIds.empty())
    {
        id = mSourceIds.top();
        mSourceIds.pop();
    }
    else
    {
        alGetError();
        alGenSources(1, &id);
        if(alGetError() != AL_NO_ERROR)
        {
            // FIXME: Steal from an ALSource
            throw std::runtime_error("No source IDs");
        }
    }
    return id;
}


Source *ALContext::getSource()
{
    CheckContext(this);

    ALSource *source = 0;
    if(mFreeSources.empty())
        source = new ALSource(this);
    else
    {
        source = mFreeSources.back();
        mFreeSources.pop();
    }
    mUsedSources.insert(source);
    return source;
}

void ALContext::freeSource(ALSource *source)
{
    mUsedSources.erase(source);
    mFreeSources.push(source);
}

void ALContext::finalize(Source *source)
{
    ALSource *alsrc = dynamic_cast<ALSource*>(source);
    if(!alsrc) throw std::runtime_error("Invalid source");
    CheckContext(this);
    alsrc->finalize();
}


void ALContext::update()
{
    CheckContext(this);
    std::for_each(mUsedSources.begin(), mUsedSources.end(), std::mem_fun(&ALSource::updateNoCtxCheck));
}


void Context::MakeCurrent(Context *context)
{
    ALContext *ctx = 0;
    if(context)
    {
        ctx = dynamic_cast<ALContext*>(context);
        if(!ctx) throw std::runtime_error("Invalid context pointer");
    }
    ALContext::MakeCurrent(ctx);
}

Context *Context::GetCurrent()
{
    return ALContext::GetCurrent();
}

void Context::MakeThreadCurrent(Context *context)
{
    ALContext *ctx = 0;
    if(context)
    {
        ctx = dynamic_cast<ALContext*>(context);
        if(!ctx) throw std::runtime_error("Invalid context pointer");
    }
    ALContext::MakeThreadCurrent(ctx);
}

Context *Context::GetThreadCurrent()
{
    return ALContext::GetThreadCurrent();
}

}
