
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
#include <new>

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
#include <sourcegroup.h>

namespace alure
{

template<typename T, size_t N>
static inline size_t countof(const T(&)[N])
{ return N; }


typedef std::pair<String,SharedPtr<DecoderFactory>> FactoryPair;
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

typedef std::map<String,SharedPtr<DecoderFactory>> FactoryMap;
static FactoryMap sDecoders;

template<typename T>
static SharedPtr<Decoder> GetDecoder(const String &name, SharedPtr<std::istream> file, T start, T end)
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

static SharedPtr<Decoder> GetDecoder(const String &name, SharedPtr<std::istream> file)
{
    SharedPtr<Decoder> decoder = GetDecoder(name, file, sDecoders.begin(), sDecoders.end());
    if(!decoder) decoder = GetDecoder(name, file, std::begin(sDefaultDecoders), std::end(sDefaultDecoders));
    if(!decoder) throw std::runtime_error("No decoder for "+name);
    return decoder;
}

void RegisterDecoder(const String &name, SharedPtr<DecoderFactory> factory)
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

SharedPtr<DecoderFactory> UnregisterDecoder(const String &name)
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
    virtual SharedPtr<std::istream> openFile(const String &name)
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


// Default message handler methods are no-ops.
MessageHandler::~MessageHandler()
{
}

void MessageHandler::bufferLoading(const String&, ChannelConfig, SampleType, ALuint, const Vector<ALbyte>&)
{
}

bool MessageHandler::resourceNotFound(const String&, String&)
{
    return false;
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
        lock1 = std::unique_lock<std::mutex>(sCurrentCtx->mContextMutex);
    if(context && context != sCurrentCtx)
        lock2 = std::unique_lock<std::mutex>(context->mContextMutex);

    if(alcMakeContextCurrent(context ? context->getContext() : 0) == ALC_FALSE)
        throw std::runtime_error("Call to alcMakeContextCurrent failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ALContext::setupExts), context);
    }
    std::swap(sCurrentCtx, context);
    if(context) context->decRef();

    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = 0;

    if(sCurrentCtx && sCurrentCtx != context)
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

    std::chrono::steady_clock::time_point basetime = std::chrono::steady_clock::now();
    std::chrono::milliseconds waketime(0);
    std::unique_lock<std::mutex> ctxlock(mContextMutex);
    while(!mQuitThread)
    {
        {
            std::unique_lock<std::mutex> srclock(mSourceStreamMutex);
            auto source = mStreamingSources.begin();
            while(source != mStreamingSources.end())
            {
                if(!(*source)->updateAsync())
                    source = mStreamingSources.erase(source);
                else
                    ++source;
            }
            srclock.unlock();
        }

        // Only do one pending buffer at a time. In case there's several large
        // buffers to load, we still need to process streaming sources so they
        // don't underrun.
        ll_ringbuffer_data_t vec[2];
        ll_ringbuffer_get_read_vector(mPendingBuffers, vec);
        if(vec[0].len > 0)
        {
            PendingBuffer *pb = reinterpret_cast<PendingBuffer*>(vec[0].buf);
            pb->mBuffer->load(pb->mFrames, pb->mFormat, pb->mDecoder, pb->mName, this);
            pb->~PendingBuffer();
            ll_ringbuffer_read_advance(mPendingBuffers, 1);
            continue;
        }

        std::unique_lock<std::mutex> wakelock(mWakeMutex);
        if(!mQuitThread && ll_ringbuffer_read_space(mPendingBuffers) == 0)
        {
            ctxlock.unlock();

            ALuint interval = mWakeInterval.load();
            if(!interval)
                mWakeThread.wait(wakelock);
            else
            {
                auto now = std::chrono::steady_clock::now() - basetime;
                auto duration = std::chrono::milliseconds(interval);
                while((waketime - now).count() <= 0) waketime += duration;
                mWakeThread.wait_until(wakelock, waketime + basetime);
            }
            wakelock.unlock();

            ctxlock.lock();
            while(!mQuitThread && alcGetCurrentContext() != getContext())
                mWakeThread.wait(ctxlock);
        }
    }
    ctxlock.unlock();

    if(ALDeviceManager::SetThreadContext)
        ALDeviceManager::SetThreadContext(0);
}


ALContext::ALContext(ALCcontext *context, ALDevice *device)
  : mContext(context), mDevice(device), mRefs(0),
    mHasExt{false}, mPendingBuffers(nullptr), mWakeInterval(0), mQuitThread(false),
    mIsBatching(false),
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
    mPendingBuffers = ll_ringbuffer_create(16, sizeof(PendingBuffer));
}

ALContext::~ALContext()
{
    mDevice->removeContext(this);
    ll_ringbuffer_free(mPendingBuffers);
}


Device *ALContext::getDevice()
{
    return mDevice;
}

void ALContext::destroy()
{
    if(mRefs.load() != 0)
        throw std::runtime_error("Context is in use");
    if(!mBuffers.empty())
        throw std::runtime_error("Trying to destroy a context with buffers");

    if(mThread.joinable())
    {
        std::unique_lock<std::mutex> lock(mWakeMutex);
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
    mIsBatching = true;
}

void ALContext::endBatch()
{
    alcProcessContext(mContext);
    mIsBatching = false;
}


Listener *ALContext::getListener()
{
    return this;
}


SharedPtr<MessageHandler> ALContext::setMessageHandler(SharedPtr<MessageHandler> handler)
{
    std::lock_guard<std::mutex> lock(mContextMutex);
    mMessage.swap(handler);
    return handler;
}


void ALContext::setAsyncWakeInterval(ALuint msec)
{
    mWakeInterval.store(msec);
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();
}

ALuint ALContext::getAsyncWakeInterval() const
{
    return mWakeInterval.load();
}


SharedPtr<Decoder> ALContext::createDecoder(const String &name)
{
    SharedPtr<std::istream> file(FileIOFactory::get().openFile(name));
    if(file.get()) return GetDecoder(name, file);

    // Resource not found. Try to find a substitute.
    if(!mMessage.get()) throw std::runtime_error("Failed to open "+name);
    String oldname = name;
    do {
        String newname;
        if(!mMessage->resourceNotFound(oldname, newname))
            throw std::runtime_error("Failed to open "+oldname);
        file = FileIOFactory::get().openFile(newname);
        oldname = std::move(newname);
    } while(!file.get());

    return GetDecoder(oldname, file);
}


Buffer *ALContext::getBuffer(const String &name)
{
    CheckContext(this);

    BufferMap::const_iterator iter = mBuffers.find(name);
    if(iter != mBuffers.end())
    {
        // Ensure the buffer is loaded before returning. getBuffer guarantees
        // the returned buffer is loaded.
        ALBuffer *buffer = iter->second;
        while(buffer->getLoadStatus() == BufferLoad_Pending)
            std::this_thread::yield();
        return buffer;
    }

    SharedPtr<Decoder> decoder(createDecoder(name));

    ALuint srate = decoder->getFrequency();
    ChannelConfig chans = decoder->getChannelConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();

    Vector<ALbyte> data(FramesToBytes(frames, chans, type));
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

        ALBuffer *buffer = new ALBuffer(this, bid, srate, chans, type, true);
        return mBuffers.insert(std::make_pair(name, buffer)).first->second;
    }
    catch(...) {
        alDeleteBuffers(1, &bid);
        throw;
    }
}

Buffer *ALContext::getBufferAsync(const String &name)
{
    CheckContext(this);

    BufferMap::const_iterator iter = mBuffers.find(name);
    if(iter != mBuffers.end()) return iter->second;

    SharedPtr<Decoder> decoder(createDecoder(name));

    ALuint srate = decoder->getFrequency();
    ChannelConfig chans = decoder->getChannelConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();
    if(!frames) throw std::runtime_error("No samples for buffer");

    ALenum format = GetFormat(chans, type);

    alGetError();
    ALuint bid = 0;
    alGenBuffers(1, &bid);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Failed to buffer data");

    ALBuffer *buffer = new ALBuffer(this, bid, srate, chans, type, false);

    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ALContext::backgroundProc), this);

    while(ll_ringbuffer_write_space(mPendingBuffers) == 0)
        std::this_thread::yield();

    ll_ringbuffer_data_t vec[2];
    ll_ringbuffer_get_write_vector(mPendingBuffers, vec);
    new(vec[0].buf) PendingBuffer{name, buffer, decoder, format, frames};
    ll_ringbuffer_write_advance(mPendingBuffers, 1);
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();

    return mBuffers.insert(std::make_pair(name, buffer)).first->second;
}


void ALContext::removeBuffer(const String &name)
{
    CheckContext(this);
    BufferMap::iterator iter = mBuffers.find(name);
    if(iter != mBuffers.end())
    {
        ALBuffer *albuf = iter->second;
        albuf->cleanup();
        mBuffers.erase(iter);
    }
}

void ALContext::removeBuffer(Buffer *buffer)
{
    CheckContext(this);
    BufferMap::iterator iter = mBuffers.begin();
    while(iter != mBuffers.end())
    {
        ALBuffer *albuf = iter->second;
        if(albuf == buffer)
        {
            albuf->cleanup();
            mBuffers.erase(iter);
            break;
        }
        ++iter;
    }
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
    auto iter = std::lower_bound(mUsedSources.begin(), mUsedSources.end(), source);
    if(iter == mUsedSources.end() || *iter != source)
        mUsedSources.insert(iter, source);
    return source;
}

void ALContext::freeSource(ALSource *source)
{
    auto iter = std::lower_bound(mUsedSources.begin(), mUsedSources.end(), source);
    if(iter != mUsedSources.end() && *iter == source) mUsedSources.erase(iter);
    mFreeSources.push(source);
}


void ALContext::addStream(ALSource *source)
{
    std::lock_guard<std::mutex> lock(mSourceStreamMutex);
    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ALContext::backgroundProc), this);
    mStreamingSources.insert(source);
}

void ALContext::removeStream(ALSource *source)
{
    std::lock_guard<std::mutex> lock(mSourceStreamMutex);
    mStreamingSources.erase(source);
}

void ALContext::removeStreamNoLock(ALSource *source)
{
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


SourceGroup *ALContext::createSourceGroup()
{
    ALSourceGroup *group = new ALSourceGroup(this);
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), group);
    mSourceGroups.insert(iter, group);
    return group;
}

void ALContext::freeSourceGroup(ALSourceGroup *group)
{
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), group);
    if(iter != mSourceGroups.end() && *iter != group)
        mSourceGroups.erase(iter);
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
    if(!mWakeInterval.load())
    {
        // For performance reasons, don't wait for the thread's mutex. This
        // should be called often enough to keep up with any and all streams
        // regardless.
        mWakeThread.notify_all();
    }
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

void ALContext::setMetersPerUnit(ALfloat m_u)
{
    if(!(m_u > 0.0f))
        throw std::runtime_error("Invalid meters per unit");
    CheckContext(this);
    if(hasExtension(EXT_EFX))
        alListenerf(AL_METERS_PER_UNIT, m_u);
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
