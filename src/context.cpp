
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <algorithm>
#include <memory>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <map>

#include "alc.h"

#ifdef HAVE_VORBISFILE
#include "decoders/vorbisfile.hpp"
#endif
#ifdef HAVE_LIBFLAC
#include "decoders/flac.hpp"
#endif
#ifdef HAVE_OPUSFILE
#include "decoders/opusfile.hpp"
#endif
#ifdef HAVE_LIBSNDFILE
#include "decoders/sndfile.hpp"
#endif
#ifdef HAVE_MPG123
#include "decoders/mpg123.hpp"
#endif
#include "decoders/wave.hpp"

#include "devicemanager.h"
#include "device.h"
#include "buffer.h"
#include "source.h"
#include "auxeffectslot.h"
#include "effect.h"

namespace alure
{

template<typename T, size_t N>
static inline size_t countof(const T(&)[N])
{ return N; }


typedef std::pair<std::string,SharedPtr<DecoderFactory>> FactoryPair;
static const FactoryPair sDefaultDecoders[] = {
#ifdef HAVE_VORBISFILE
    { "_alure_int_vorbis", SharedPtr<DecoderFactory>(new VorbisFileDecoderFactory) },
#endif
#ifdef HAVE_LIBFLAC
    { "_alure_int_flac", SharedPtr<DecoderFactory>(new FlacDecoderFactory) },
#endif
#ifdef HAVE_OPUSFILE
    { "_alure_int_opus", SharedPtr<DecoderFactory>(new OpusFileDecoderFactory) },
#endif

    { "_alure_int_wave", SharedPtr<DecoderFactory>(new WaveDecoderFactory) },

#ifdef HAVE_LIBSNDFILE
    { "_alure_int_sndfile", SharedPtr<DecoderFactory>(new SndFileDecoderFactory) },
#endif
#ifdef HAVE_MPG123
    { "_alure_int_mpg123", SharedPtr<DecoderFactory>(new Mpg123DecoderFactory) },
#endif
};

typedef std::map<std::string,SharedPtr<DecoderFactory>> FactoryMap;
static FactoryMap sDecoders;

template<typename T1, typename T2>
static SharedPtr<Decoder> GetDecoder(const std::string &name, SharedPtr<std::istream> file, T1 start, T2 end)
{
    while(start != end)
    {
        DecoderFactory *factory = start->second.get();
        SharedPtr<Decoder> decoder = factory->createDecoder(file);
        if(decoder) return decoder;

        file->clear();
        if(!file->seekg(0))
            throw std::runtime_error("Failed to rewind "+name+" for the next decoder factory");

        ++start;
    }

    return SharedPtr<Decoder>(nullptr);
}

void RegisterDecoder(const std::string &name, SharedPtr<DecoderFactory> factory)
{
    FactoryMap::iterator iter = sDecoders.begin();
    while(iter != sDecoders.end())
    {
        if(iter->first == name)
            throw std::runtime_error("Decoder factory \""+name+"\" already registered");
        if(iter->second.get() == factory.get())
        {
            std::stringstream sstr;
            sstr<< "Decoder factory instance "<<factory<<" already registered";
            throw std::runtime_error(sstr.str());
        }
        iter++;
    }
    sDecoders.insert(std::make_pair(name, factory));
}

SharedPtr<DecoderFactory> UnregisterDecoder(const std::string &name)
{
    FactoryMap::iterator iter = sDecoders.find(name);
    if(iter != sDecoders.end())
    {
        SharedPtr<DecoderFactory> factory = iter->second;
        sDecoders.erase(iter);
        return factory;
    }
    return SharedPtr<DecoderFactory>(nullptr);
}


class DefaultFileIOFactory : public FileIOFactory {
    virtual SharedPtr<std::istream> openFile(const std::string &name)
    {
        SharedPtr<std::ifstream> file(new std::ifstream(name.c_str(), std::ios::binary));
        if(!file->is_open()) file.reset();
        return file;
    }
};
static DefaultFileIOFactory sDefaultFileFactory;

static SharedPtr<FileIOFactory> sFileFactory;
SharedPtr<FileIOFactory> FileIOFactory::set(SharedPtr<FileIOFactory> factory)
{
    sFileFactory.swap(factory);
    return factory;
}

FileIOFactory &FileIOFactory::get()
{
    FileIOFactory *factory = sFileFactory.get();
    if(factory) return *factory;
    return sDefaultFileFactory;
}


template<typename T>
static inline void LoadALFunc(T **func, const char *name)
{ *func = reinterpret_cast<T*>(alGetProcAddress(name)); }


static void LoadNothing(ALContext*) { }

static void LoadEFX(ALContext *ctx)
{
    LoadALFunc(&ctx->alGenEffects,    "alGenEffects");
    LoadALFunc(&ctx->alDeleteEffects, "alDeleteEffects");
    LoadALFunc(&ctx->alIsEffect,      "alIsEffect");
    LoadALFunc(&ctx->alEffecti,       "alEffecti");
    LoadALFunc(&ctx->alEffectiv,      "alEffectiv");
    LoadALFunc(&ctx->alEffectf,       "alEffectf");
    LoadALFunc(&ctx->alEffectfv,      "alEffectfv");
    LoadALFunc(&ctx->alGetEffecti,    "alGetEffecti");
    LoadALFunc(&ctx->alGetEffectiv,   "alGetEffectiv");
    LoadALFunc(&ctx->alGetEffectf,    "alGetEffectf");
    LoadALFunc(&ctx->alGetEffectfv,   "alGetEffectfv");

    LoadALFunc(&ctx->alGenFilters,    "alGenFilters");
    LoadALFunc(&ctx->alDeleteFilters, "alDeleteFilters");
    LoadALFunc(&ctx->alIsFilter,      "alIsFilter");
    LoadALFunc(&ctx->alFilteri,       "alFilteri");
    LoadALFunc(&ctx->alFilteriv,      "alFilteriv");
    LoadALFunc(&ctx->alFilterf,       "alFilterf");
    LoadALFunc(&ctx->alFilterfv,      "alFilterfv");
    LoadALFunc(&ctx->alGetFilteri,    "alGetFilteri");
    LoadALFunc(&ctx->alGetFilteriv,   "alGetFilteriv");
    LoadALFunc(&ctx->alGetFilterf,    "alGetFilterf");
    LoadALFunc(&ctx->alGetFilterfv,   "alGetFilterfv");

    LoadALFunc(&ctx->alGenAuxiliaryEffectSlots,    "alGenAuxiliaryEffectSlots");
    LoadALFunc(&ctx->alDeleteAuxiliaryEffectSlots, "alDeleteAuxiliaryEffectSlots");
    LoadALFunc(&ctx->alIsAuxiliaryEffectSlot,      "alIsAuxiliaryEffectSlot");
    LoadALFunc(&ctx->alAuxiliaryEffectSloti,       "alAuxiliaryEffectSloti");
    LoadALFunc(&ctx->alAuxiliaryEffectSlotiv,      "alAuxiliaryEffectSlotiv");
    LoadALFunc(&ctx->alAuxiliaryEffectSlotf,       "alAuxiliaryEffectSlotf");
    LoadALFunc(&ctx->alAuxiliaryEffectSlotfv,      "alAuxiliaryEffectSlotfv");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSloti,    "alGetAuxiliaryEffectSloti");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSlotiv,   "alGetAuxiliaryEffectSlotiv");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSlotf,    "alGetAuxiliaryEffectSlotf");
    LoadALFunc(&ctx->alGetAuxiliaryEffectSlotfv,   "alGetAuxiliaryEffectSlotfv");
}

static void LoadSourceLatency(ALContext *ctx)
{
    LoadALFunc(&ctx->alGetSourcei64vSOFT, "alGetSourcei64vSOFT");
}

static const struct {
    enum ALExtension extension;
    const char name[32];
    void (*loader)(ALContext*);
} ALExtensionList[] = {
    { EXT_EFX, "ALC_EXT_EFX", LoadEFX },

    { EXT_FLOAT32,   "AL_EXT_FLOAT32",   LoadNothing },
    { EXT_MCFORMATS, "AL_EXT_MCFORMATS", LoadNothing },
    { EXT_BFORMAT,   "AL_EXT_BFORMAT",   LoadNothing },

    { EXT_MULAW,           "AL_EXT_MULAW",           LoadNothing },
    { EXT_MULAW_MCFORMATS, "AL_EXT_MULAW_MCFORMATS", LoadNothing },
    { EXT_MULAW_BFORMAT,   "AL_EXT_MULAW_BFORMAT",   LoadNothing },

    { SOFT_loop_points,    "AL_SOFT_loop_points",    LoadNothing },
    { SOFT_source_latency, "AL_SOFT_source_latency", LoadSourceLatency },
};


ALContext *ALContext::sCurrentCtx = 0;
thread_local ALContext *ALContext::sThreadCurrentCtx;

void ALContext::MakeCurrent(ALContext *context)
{
    std::unique_lock<std::mutex> lock1, lock2;
    if(sCurrentCtx)
        lock1 = std::unique_lock<std::mutex>(sCurrentCtx->mMutex);
    if(context && context != sCurrentCtx)
        lock2 = std::unique_lock<std::mutex>(context->mMutex);

    if(alcMakeContextCurrent(context ? context->getContext() : 0) == ALC_FALSE)
        throw std::runtime_error("Call to alcMakeContextCurrent failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ALContext::setupExts), context);
    }
    if(sCurrentCtx)
        sCurrentCtx->decRef();
    sCurrentCtx = context;
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = 0;

    if(sCurrentCtx)
    {
        lock2.unlock();
        sCurrentCtx->mWakeThread.notify_all();
    }
}

void ALContext::MakeThreadCurrent(ALContext *context)
{
    if(!ALDeviceManager::SetThreadContext)
        throw std::runtime_error("Thread-local contexts unsupported");
    if(ALDeviceManager::SetThreadContext(context ? context->getContext() : 0) == ALC_FALSE)
        throw std::runtime_error("Call to alcSetThreadContext failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ALContext::setupExts), context);
    }
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = context;
}

void ALContext::setupExts()
{
    ALCdevice *device = mDevice->getDevice();
    for(size_t i = 0;i < countof(ALExtensionList);i++)
    {
        mHasExt[ALExtensionList[i].extension] = false;
        if(strncmp(ALExtensionList[i].name, "ALC", 3) == 0)
            mHasExt[ALExtensionList[i].extension] = alcIsExtensionPresent(device, ALExtensionList[i].name);
        else
            mHasExt[ALExtensionList[i].extension] = alIsExtensionPresent(ALExtensionList[i].name);

        if(mHasExt[ALExtensionList[i].extension])
            ALExtensionList[i].loader(this);
    }
}


void ALContext::backgroundProc()
{
    if(ALDeviceManager::SetThreadContext && mDevice->hasExtension(EXT_thread_local_context))
        ALDeviceManager::SetThreadContext(getContext());

    std::unique_lock<std::mutex> lock(mMutex);
    while(!mQuitThread)
    {
        for(PendingBuffer &pendbuf : mPendingBuffers)
            pendbuf.mBuffer->load(pendbuf.mFrames, pendbuf.mFormat,
                                  hasExtension(SOFT_loop_points),
                                  pendbuf.mDecoder, pendbuf.mName);
        mPendingBuffers.clear();

        auto source = mStreamingSources.begin();
        while(source != mStreamingSources.end())
        {
            if(!(*source)->updateAsync())
                source = mStreamingSources.erase(source);
            else
                ++source;
        }

        do {
            mWakeThread.wait(lock);
        } while(alcGetCurrentContext() != getContext());
    }
    lock.unlock();

    if(ALDeviceManager::SetThreadContext)
        ALDeviceManager::SetThreadContext(0);
}


ALContext::ALContext(ALCcontext *context, ALDevice *device)
  : mContext(context), mDevice(device), mRefs(0),
    mHasExt{false}, mQuitThread(false),
    alGetSourcei64vSOFT(0),
    alGenEffects(0), alDeleteEffects(0), alIsEffect(0),
    alEffecti(0), alEffectiv(0), alEffectf(0), alEffectfv(0),
    alGetEffecti(0), alGetEffectiv(0), alGetEffectf(0), alGetEffectfv(0),
    alGenFilters(0), alDeleteFilters(0), alIsFilter(0),
    alFilteri(0), alFilteriv(0), alFilterf(0), alFilterfv(0),
    alGetFilteri(0), alGetFilteriv(0), alGetFilterf(0), alGetFilterfv(0),
    alGenAuxiliaryEffectSlots(0), alDeleteAuxiliaryEffectSlots(0), alIsAuxiliaryEffectSlot(0),
    alAuxiliaryEffectSloti(0), alAuxiliaryEffectSlotiv(0), alAuxiliaryEffectSlotf(0), alAuxiliaryEffectSlotfv(0),
    alGetAuxiliaryEffectSloti(0), alGetAuxiliaryEffectSlotiv(0), alGetAuxiliaryEffectSlotf(0), alGetAuxiliaryEffectSlotfv(0)
{
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

    if(mThread.joinable())
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mQuitThread = true;
        lock.unlock();
        mWakeThread.notify_all();
        mThread.join();
    }

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


Listener *ALContext::getListener()
{
    return this;
}


SharedPtr<MessageHandler> ALContext::setMessageHandler(SharedPtr<MessageHandler> handler)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mMessage.swap(handler);
    return handler;
}

SharedPtr<MessageHandler> ALContext::getMessageHandler() const
{
    return mMessage;
}


SharedPtr<Decoder> ALContext::createDecoder(const std::string &name)
{
    SharedPtr<std::istream> file(FileIOFactory::get().openFile(name));
    if(!file.get()) throw std::runtime_error("Failed to open "+name);

    SharedPtr<Decoder> decoder = GetDecoder(name, file, sDecoders.begin(), sDecoders.end());
    if(!decoder) decoder = GetDecoder(name, file, std::begin(sDefaultDecoders), std::end(sDefaultDecoders));
    if(decoder) return decoder;

    throw std::runtime_error("No decoder for "+name);
}


Buffer *ALContext::getBuffer(const std::string &name)
{
    CheckContext(this);

    Buffer *buffer = mDevice->getBuffer(name);
    if(buffer)
    {
        // Ensure the buffer is loaded before returning. getBuffer guarantees
        // the returned buffer is loaded.
        while(buffer->getLoadStatus() == BufferLoad_Pending)
            std::this_thread::yield();
        return buffer;
    }

    SharedPtr<Decoder> decoder(createDecoder(name));

    ALuint srate = decoder->getFrequency();
    SampleConfig chans = decoder->getSampleConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();

    std::vector<ALbyte> data(FramesToBytes(frames, chans, type));
    frames = decoder->read(&data[0], frames);
    if(!frames) throw std::runtime_error("No samples for buffer");
    data.resize(FramesToBytes(frames, chans, type));

    std::pair<uint64_t,uint64_t> loop_pts = decoder->getLoopPoints();
    if(loop_pts.first >= loop_pts.second)
        loop_pts = std::make_pair(0, frames);
    else
    {
        loop_pts.second = std::min<uint64_t>(loop_pts.second, frames);
        loop_pts.first = std::min<uint64_t>(loop_pts.first, loop_pts.second-1);
    }

    // Get the format before calling the bufferLoading message handler, to
    // ensure it's something OpenAL can handle.
    ALenum format = GetFormat(chans, type);

    if(mMessage.get())
        mMessage->bufferLoading(name, chans, type, srate, data);

    alGetError();
    ALuint bid = 0;
    try {
        alGenBuffers(1, &bid);
        alBufferData(bid, format, &data[0], data.size(), srate);
        if(hasExtension(SOFT_loop_points))
        {
            ALint pts[2]{(ALint)loop_pts.first, (ALint)loop_pts.second};
            alBufferiv(bid, AL_LOOP_POINTS_SOFT, pts);
        }
        if(alGetError() != AL_NO_ERROR)
            throw std::runtime_error("Failed to buffer data");

        return mDevice->addBuffer(name, new ALBuffer(mDevice, bid, srate, chans, type, true));
    }
    catch(...) {
        alDeleteBuffers(1, &bid);
        throw;
    }
}

Buffer *ALContext::getBufferAsync(const std::string &name)
{
    CheckContext(this);

    Buffer *buffer = mDevice->getBuffer(name);
    if(buffer) return buffer;

    SharedPtr<Decoder> decoder(createDecoder(name));

    ALuint srate = decoder->getFrequency();
    SampleConfig chans = decoder->getSampleConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();
    if(!frames) throw std::runtime_error("No samples for buffer");

    ALenum format = GetFormat(chans, type);

    alGetError();
    ALuint bid = 0;
    alGenBuffers(1, &bid);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Failed to buffer data");

    ALBuffer *newbuf = new ALBuffer(mDevice, bid, srate, chans, type, false);

    if(!decoder->isThreadSafe())
        newbuf->load(frames, format, hasExtension(SOFT_loop_points), decoder, name);
    else
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if(mThread.get_id() == std::thread::id())
            mThread = std::thread(std::mem_fn(&ALContext::backgroundProc), this);
        mPendingBuffers.push_back(PendingBuffer{name, newbuf, decoder, format, frames});
        lock.unlock();
        mWakeThread.notify_all();
    }

    return mDevice->addBuffer(name, newbuf);
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


ALuint ALContext::getSourceId(ALuint maxprio)
{
    CheckContext(this);

    ALuint id = 0;
    if(mSourceIds.empty())
    {
        alGetError();
        alGenSources(1, &id);
        if(alGetError() == AL_NO_ERROR)
            return id;

        ALSource *lowest = 0;
        for(ALSource *src : mUsedSources)
        {
            if(src->getId() != 0 && (!lowest || src->getPriority() < lowest->getPriority()))
                lowest = src;
        }
        if(lowest && lowest->getPriority() < maxprio)
            lowest->stop();
    }
    if(mSourceIds.empty())
        throw std::runtime_error("No available sources");

    id = mSourceIds.top();
    mSourceIds.pop();
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


void ALContext::addStream(ALSource *source)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ALContext::backgroundProc), this);
    mStreamingSources.insert(source);
}

void ALContext::removeStream(ALSource *source)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mStreamingSources.erase(source);
}


AuxiliaryEffectSlot *ALContext::createAuxiliaryEffectSlot()
{
    if(!hasExtension(EXT_EFX) || !alGenAuxiliaryEffectSlots)
        throw std::runtime_error("AuxiliaryEffectSlots not supported");
    CheckContext(this);

    alGetError();
    ALuint id = 0;
    alGenAuxiliaryEffectSlots(1, &id);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Failed to create AuxiliaryEffectSlot");
    try {
        return new ALAuxiliaryEffectSlot(this, id);
    }
    catch(...) {
        alDeleteAuxiliaryEffectSlots(1, &id);
        throw;
    }
}


Effect *ALContext::createEffect()
{
    if(!hasExtension(EXT_EFX))
        throw std::runtime_error("Effects not supported");
    CheckContext(this);

    alGetError();
    ALuint id = 0;
    alGenEffects(1, &id);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Failed to create Effect");
    try {
        return new ALEffect(this, id);
    }
    catch(...) {
        alDeleteEffects(1, &id);
        throw;
    }
}


void ALContext::setDopplerFactor(ALfloat factor)
{
    if(!(factor >= 0.0f))
        throw std::runtime_error("Doppler factor out of range");
    CheckContext(this);
    alDopplerFactor(factor);
}


void ALContext::setSpeedOfSound(ALfloat speed)
{
    if(!(speed > 0.0f))
        throw std::runtime_error("Speed of sound out of range");
    CheckContext(this);
    alSpeedOfSound(speed);
}


void ALContext::setDistanceModel(DistanceModel model)
{
    CheckContext(this);
    alDistanceModel(model);
}


void ALContext::update()
{
    CheckContext(this);
    std::for_each(mUsedSources.begin(), mUsedSources.end(), std::mem_fun(&ALSource::updateNoCtxCheck));
    mWakeThread.notify_all();
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


void Context::MakeCurrent(Context *context)
{
    ALContext *ctx = 0;
    if(context)
    {
        ctx = cast<ALContext*>(context);
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
        ctx = cast<ALContext*>(context);
        if(!ctx) throw std::runtime_error("Invalid context pointer");
    }
    ALContext::MakeThreadCurrent(ctx);
}

Context *Context::GetThreadCurrent()
{
    return ALContext::GetThreadCurrent();
}

}
