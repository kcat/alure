
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <memory>

#include "alc.h"
#include "alext.h"

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
    if((thrdctx && device != thrdctx->getDevice()) || (!thrdctx && !globctx) || (!thrdctx && device != globctx->getDevice()))
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
        throw std::runtime_error("Call too alcSetThreadContext failed");
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


static ALuint FramesToBytes(ALuint size, SampleConfig chans, SampleType type)
{
    switch(chans)
    {
        case SampleConfig_Mono: size *= 1; break;
        case SampleConfig_Stereo: size *= 2; break;
        case SampleConfig_Rear: size *= 2; break;
        case SampleConfig_Quad: size *= 4; break;
        case SampleConfig_X51: size *= 6; break;
        case SampleConfig_X61: size *= 7; break;
        case SampleConfig_X71: size *= 8; break;
    }
    switch(type)
    {
        case SampleType_UInt8: size *= 1; break;
        case SampleType_Int16: size *= 2; break;
        case SampleType_Float32: size *= 4; break;
    }

    return size;
}

static ALenum GetFormat(SampleConfig chans, SampleType type)
{
    ALenum format = AL_NONE;

    if(type == SampleType_UInt8)
    {
        if(chans == SampleConfig_Mono) format = AL_FORMAT_MONO8;
        else if(chans == SampleConfig_Stereo) format = AL_FORMAT_STEREO8;
        else if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) format = alGetEnumValue("AL_FORMAT_REAR8");
            else if(chans == SampleConfig_Quad) format = alGetEnumValue("AL_FORMAT_QUAD8");
            else if(chans == SampleConfig_X51) format = alGetEnumValue("AL_FORMAT_51CHN8");
            else if(chans == SampleConfig_X61) format = alGetEnumValue("AL_FORMAT_61CHN8");
            else if(chans == SampleConfig_X71) format = alGetEnumValue("AL_FORMAT_71CHN8");
        }
    }
    else if(type == SampleType_Int16)
    {
        if(chans == SampleConfig_Mono) format = AL_FORMAT_MONO16;
        else if(chans == SampleConfig_Stereo) format = AL_FORMAT_STEREO16;
        else if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) format = alGetEnumValue("AL_FORMAT_REAR16");
            else if(chans == SampleConfig_Quad) format = alGetEnumValue("AL_FORMAT_QUAD16");
            else if(chans == SampleConfig_X51) format = alGetEnumValue("AL_FORMAT_51CHN16");
            else if(chans == SampleConfig_X61) format = alGetEnumValue("AL_FORMAT_61CHN16");
            else if(chans == SampleConfig_X71) format = alGetEnumValue("AL_FORMAT_71CHN16");
        }
    }
    else if(type == SampleType_Float32 && alIsExtensionPresent("AL_EXT_float32"))
    {
        if(chans == SampleConfig_Mono) format = AL_FORMAT_MONO_FLOAT32;
        else if(chans == SampleConfig_Stereo) format = AL_FORMAT_STEREO_FLOAT32;
        else if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
        {
            if(chans == SampleConfig_Rear) format = alGetEnumValue("AL_FORMAT_REAR32");
            else if(chans == SampleConfig_Quad) format = alGetEnumValue("AL_FORMAT_QUAD32");
            else if(chans == SampleConfig_X51) format = alGetEnumValue("AL_FORMAT_51CHN32");
            else if(chans == SampleConfig_X61) format = alGetEnumValue("AL_FORMAT_61CHN32");
            else if(chans == SampleConfig_X71) format = alGetEnumValue("AL_FORMAT_71CHN32");
        }
    }

    if(format == AL_NONE || format == -1)
        throw std::runtime_error("Format not supported");
    return format;
}

Buffer *ALContext::getBuffer(const std::string &name)
{
    CheckContext(this);

    Buffer *buffer = mDevice->getBuffer(name);
    if(buffer) return buffer;

#ifdef HAVE_LIBSNDFILE
    std::auto_ptr<Decoder> decoder(SndFileDecoder::openFile(name.c_str()));

    ALuint srate;
    SampleConfig chans;
    SampleType type;
    decoder->getFormat(&srate, &chans, &type);

    ALuint frames = decoder->getLength();
    std::vector<ALbyte> data(FramesToBytes(frames, chans, type));
    frames = decoder->read(&data[0], frames);
    data.resize(FramesToBytes(frames, chans, type));
    decoder.reset();

    alGetError();

    ALuint bid = 0;
    alGenBuffers(1, &bid);
    if(alGetError() != AL_NO_ERROR || bid == 0)
        throw std::runtime_error("Failed to get buffer ID");
    try {
        alBufferData(bid, GetFormat(chans, type), &data[0], data.size(), srate);
        if(alGetError() != AL_NO_ERROR)
            throw std::runtime_error("Failed to buffer data");

        return mDevice->addBuffer(name, new ALBuffer(mDevice, bid));
    }
    catch(...) {
        alDeleteBuffers(1, &bid);
        throw;
    }
#endif

    throw std::runtime_error("No decoder for file");
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
    if(mSources.empty())
        source = new ALSource(this);
    else
    {
        source = mSources.back();
        mSources.pop();
    }
    return source;
}

void ALContext::finalize(Source *source)
{
    ALSource *alsrc = dynamic_cast<ALSource*>(source);
    if(!alsrc) throw std::runtime_error("Invalid source");
    alsrc->finalize();
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
