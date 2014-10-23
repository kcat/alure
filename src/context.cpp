
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <algorithm>
#include <memory>

#include "alc.h"

#ifdef HAVE_LIBSNDFILE
#include "decoders/sndfile1.h"
#endif
#ifdef HAVE_MPG123
#include "decoders/mpg123-1.h"
#endif

#include "devicemanager.h"
#include "device.h"
#include "buffer.h"
#include "source.h"

namespace alure
{

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
    Decoder *decoder = 0;
#ifdef HAVE_LIBSNDFILE
    decoder = SndFileDecoder::openFile(name);
    if(decoder) return decoder;
#endif
#ifdef HAVE_MPG123
    decoder = Mpg123Decoder::openFile(name);
    if(decoder) return decoder;
#endif
    throw std::runtime_error("No decoder for "+name);
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


void ALContext::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(this);
    alListenerf(AL_GAIN, gain);
}


void ALContext::setPosition(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(this);
    alListener3f(AL_POSITION, x, y, z);
}

void ALContext::setPosition(const ALfloat *pos)
{
    CheckContext(this);
    alListenerfv(AL_POSITION, pos);
}

void ALContext::setVelocity(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(this);
    alListener3f(AL_VELOCITY, x, y, z);
}

void ALContext::setVelocity(const ALfloat *vel)
{
    CheckContext(this);
    alListenerfv(AL_VELOCITY, vel);
}

void ALContext::setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2)
{
    CheckContext(this);
    ALfloat ori[6] = { x1, y1, z1, x2, y2, z2 };
    alListenerfv(AL_ORIENTATION, ori);
}

void ALContext::setOrientation(const ALfloat *at, const ALfloat *up)
{
    CheckContext(this);
    ALfloat ori[6] = { at[0], at[1], at[2], up[0], up[1], up[2] };
    alListenerfv(AL_ORIENTATION, ori);
}

void ALContext::setOrientation(const ALfloat *ori)
{
    CheckContext(this);
    alListenerfv(AL_ORIENTATION, ori);
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
