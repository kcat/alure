
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <algorithm>
#include <memory>
#include <sstream>
#include <fstream>
#include <cstring>

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
#include "auxeffectslot.h"

namespace alure
{

typedef std::pair<std::string,std::unique_ptr<DecoderFactory>> FactoryPair;
typedef std::vector<FactoryPair> FactoryMap;

static FactoryPair sDefaultDecoders[] = {
#ifdef HAVE_MPG123
    { "_alure_int_mpg123", std::unique_ptr<DecoderFactory>(new Mpg123DecoderFactory) },
#endif
#ifdef HAVE_LIBSNDFILE
    { "_alure_int_sndfile", std::unique_ptr<DecoderFactory>(new SndFileDecoderFactory) },
#endif
};
static FactoryMap sDecoders{ std::make_move_iterator(std::begin(sDefaultDecoders)),
                             std::make_move_iterator(std::end(sDefaultDecoders)) };

void RegisterDecoder(const std::string &name, DecoderFactory *factory)
{
    FactoryMap::iterator iter = sDecoders.begin();
    while(iter != sDecoders.end())
    {
        if(iter->first == name)
            throw std::runtime_error("Decoder factory \""+name+"\" already registered");
        if(iter->second.get() == factory)
        {
            std::stringstream sstr;
            sstr<< "Decoder factory instance "<<factory<<" already registered";
            throw std::runtime_error(sstr.str());
        }
        iter++;
    }
    sDecoders.push_back(std::make_pair(name, std::unique_ptr<DecoderFactory>(factory)));
}

std::unique_ptr<DecoderFactory> UnregisterDecoder(const std::string &name)
{
    FactoryMap::iterator iter = sDecoders.begin();
    while(iter != sDecoders.end())
    {
        if(iter->first == name)
        {
            std::unique_ptr<DecoderFactory> factory = std::move(iter->second);
            sDecoders.erase(iter);
            return factory;
        }
        iter++;
    }
    return std::unique_ptr<DecoderFactory>(nullptr);
}


class DefaultFileIOFactory : public FileIOFactory {
    virtual std::unique_ptr<std::istream> createFile(const std::string &name)
    {
        std::ifstream *file = new std::ifstream(name.c_str(), std::ios::binary);
        if(!file->is_open())
        {
            delete file;
            file = 0;
        }
        return std::unique_ptr<std::istream>(file);
    }
};
static DefaultFileIOFactory sDefaultFileFactory;

static std::unique_ptr<FileIOFactory> sFileFactory;
std::unique_ptr<FileIOFactory> FileIOFactory::set(std::unique_ptr<FileIOFactory> factory)
{
    std::unique_ptr<FileIOFactory> old = std::move(sFileFactory);
    sFileFactory = std::move(factory);
    return old;
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
    {
        context->addRef();
        context->setCurrent();
    }
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
    {
        context->addRef();
        context->setCurrent();
    }
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = context;
}

void ALContext::setupExts()
{
    ALCdevice *device = mDevice->getDevice();
    if(alcIsExtensionPresent(device, "ALC_EXT_EFX"))
    {
        mHasExt[EXT_EFX] = true;
        LoadALFunc(&alGenEffects,    "alGenEffects");
        LoadALFunc(&alDeleteEffects, "alDeleteEffects");
        LoadALFunc(&alIsEffect,      "alIsEffect");
        LoadALFunc(&alEffecti,       "alEffecti");
        LoadALFunc(&alEffectiv,      "alEffectiv");
        LoadALFunc(&alEffectf,       "alEffectf");
        LoadALFunc(&alEffectfv,      "alEffectfv");
        LoadALFunc(&alGetEffecti,    "alGetEffecti");
        LoadALFunc(&alGetEffectiv,   "alGetEffectiv");
        LoadALFunc(&alGetEffectf,    "alGetEffectf");
        LoadALFunc(&alGetEffectfv,   "alGetEffectfv");
        LoadALFunc(&alGenFilters,    "alGenFilters");
        LoadALFunc(&alDeleteFilters, "alDeleteFilters");
        LoadALFunc(&alIsFilter,      "alIsFilter");
        LoadALFunc(&alFilteri,       "alFilteri");
        LoadALFunc(&alFilteriv,      "alFilteriv");
        LoadALFunc(&alFilterf,       "alFilterf");
        LoadALFunc(&alFilterfv,      "alFilterfv");
        LoadALFunc(&alGetFilteri,    "alGetFilteri");
        LoadALFunc(&alGetFilteriv,   "alGetFilteriv");
        LoadALFunc(&alGetFilterf,    "alGetFilterf");
        LoadALFunc(&alGetFilterfv,   "alGetFilterfv");
        LoadALFunc(&alGenAuxiliaryEffectSlots,    "alGenAuxiliaryEffectSlots");
        LoadALFunc(&alDeleteAuxiliaryEffectSlots, "alDeleteAuxiliaryEffectSlots");
        LoadALFunc(&alIsAuxiliaryEffectSlot,      "alIsAuxiliaryEffectSlot");
        LoadALFunc(&alAuxiliaryEffectSloti,       "alAuxiliaryEffectSloti");
        LoadALFunc(&alAuxiliaryEffectSlotiv,      "alAuxiliaryEffectSlotiv");
        LoadALFunc(&alAuxiliaryEffectSlotf,       "alAuxiliaryEffectSlotf");
        LoadALFunc(&alAuxiliaryEffectSlotfv,      "alAuxiliaryEffectSlotfv");
        LoadALFunc(&alGetAuxiliaryEffectSloti,    "alGetAuxiliaryEffectSloti");
        LoadALFunc(&alGetAuxiliaryEffectSlotiv,   "alGetAuxiliaryEffectSlotiv");
        LoadALFunc(&alGetAuxiliaryEffectSlotf,    "alGetAuxiliaryEffectSlotf");
        LoadALFunc(&alGetAuxiliaryEffectSlotfv,   "alGetAuxiliaryEffectSlotfv");
    }

    mHasExt[EXT_FLOAT32] = alIsExtensionPresent("AL_EXT_FLOAT32");
    mHasExt[EXT_MCFORMATS] = alIsExtensionPresent("AL_EXT_MCFORMATS");
    mHasExt[EXT_BFORMAT] = alIsExtensionPresent("AL_EXT_BFORMAT");

    mHasExt[EXT_MULAW] = alIsExtensionPresent("AL_EXT_MULAW");
    mHasExt[EXT_MULAW_MCFORMATS] = alIsExtensionPresent("AL_EXT_MULAW_MCFORMATS");
    mHasExt[EXT_MULAW_BFORMAT] = alIsExtensionPresent("AL_EXT_MULAW_BFORMAT");

    if(alIsExtensionPresent("AL_SOFT_source_latency"))
    {
        mHasExt[SOFT_source_latency] = true;
        LoadALFunc(&alGetSourcei64vSOFT, "alGetSourcei64vSOFT");
    }
}


ALContext::ALContext(ALCcontext *context, ALDevice *device)
  : mContext(context), mDevice(device), mRefs(0), mFirstSet(true), alGetSourcei64vSOFT(0),
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
    memset(mHasExt, 0, sizeof(mHasExt));
}

ALContext::~ALContext()
{
    mDevice->removeContext(this);
}

void ALContext::setCurrent()
{
    if(mFirstSet)
    {
        mFirstSet = false;
        setupExts();
    }
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


Listener *ALContext::getListener()
{
    return this;
}


std::unique_ptr<Decoder> ALContext::createDecoder(const std::string &name)
{
    std::unique_ptr<std::istream> file(FileIOFactory::get().createFile(name));
    if(!file.get()) throw std::runtime_error("Failed to open "+name);

    FactoryMap::const_reverse_iterator iter = sDecoders.rbegin();
    while(iter != sDecoders.rend())
    {
        DecoderFactory *factory = iter->second.get();
        Decoder *decoder = factory->createDecoder(file);
        if(decoder) return std::unique_ptr<Decoder>(decoder);

        if(!file.get())
            throw std::runtime_error("Decoder factory took file but did not give a decoder");
        file->clear();
        if(!file->seekg(0))
            throw std::runtime_error("Failed to rewind "+name+" for the next decoder factory");

        ++iter;
    }
    throw std::runtime_error("No decoder for "+name);
}


Buffer *ALContext::getBuffer(const std::string &name)
{
    CheckContext(this);

    Buffer *buffer = mDevice->getBuffer(name);
    if(buffer) return buffer;

    std::unique_ptr<Decoder> decoder(createDecoder(name));

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

        return mDevice->addBuffer(name, new ALBuffer(mDevice, bid, srate, chans, type));
    }
    catch(...) {
        alDeleteBuffers(1, &bid);
        throw;
    }
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

void ALContext::removeAuxiliaryEffectSlot(AuxiliaryEffectSlot *auxslot)
{
    ALAuxiliaryEffectSlot *slot = dynamic_cast<ALAuxiliaryEffectSlot*>(auxslot);
    if(!slot) throw std::runtime_error("Invalid AuxiliaryEffectSlot");
    slot->cleanup();
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
