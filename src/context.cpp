
#include "config.h"

#include "context.h"

#include <stdexcept>
#include <algorithm>
#include <functional>
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
#include "sourcegroup.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace std
{

// Implements a FNV-1a hash for StringView. NOTE: This is *NOT* guaranteed
// compatible with std::hash<String>! The standard does not give any specific
// hash implementation, nor a way for applications to access the same hash
// function as std::string (short of copying into a string and hashing that).
// So if you need Strings and StringViews to result in the same hash for the
// same set of characters, hash StringViews created from the Strings.
template<>
struct hash<alure::StringView> {
    size_t operator()(const alure::StringView &str) const noexcept
    {
        if /*constexpr*/ (sizeof(size_t) == 8)
        {
            static constexpr size_t hash_offset = 0xcbf29ce484222325;
            static constexpr size_t hash_prime = 0x100000001b3;

            size_t val = hash_offset;
            for(auto ch : str)
                val = (val^static_cast<unsigned char>(ch)) * hash_prime;
            return val;
        }
        else
        {
            static constexpr size_t hash_offset = 0x811c9dc5;
            static constexpr size_t hash_prime = 0x1000193;

            size_t val = hash_offset;
            for(auto ch : str)
                val = (val^static_cast<unsigned char>(ch)) * hash_prime;
            return val;
        }
    }
};

}

namespace
{

// Global mutex to protect global context changes
std::mutex mGlobalCtxMutex;

#ifdef _WIN32
// Windows' std::ifstream fails with non-ANSI paths since the standard only
// specifies names using const char* (or std::string). MSVC has a non-standard
// extension using const wchar_t* (or std::wstring?) to handle Unicode paths,
// but not all Windows compilers support it. So we have to make our own istream
// that accepts UTF-8 paths and forwards to Unicode-aware I/O functions.
class StreamBuf final : public std::streambuf {
    alure::Array<char_type,4096> mBuffer;
    HANDLE mFile;

    int_type underflow() override
    {
        if(mFile != INVALID_HANDLE_VALUE && gptr() == egptr())
        {
            // Read in the next chunk of data, and set the pointers on success
            DWORD got = 0;
            if(!ReadFile(mFile, mBuffer.data(), mBuffer.size(), &got, NULL))
                got = 0;
            setg(mBuffer.data(), mBuffer.data(), mBuffer.data()+got);
        }
        if(gptr() == egptr())
            return traits_type::eof();
        return traits_type::to_int_type(*gptr());
    }

    pos_type seekoff(off_type offset, std::ios_base::seekdir whence, std::ios_base::openmode mode) override
    {
        if(mFile == INVALID_HANDLE_VALUE || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        LARGE_INTEGER fpos;
        switch(whence)
        {
            case std::ios_base::beg:
                fpos.QuadPart = offset;
                if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_BEGIN))
                    return traits_type::eof();
                break;

            case std::ios_base::cur:
                // If the offset remains in the current buffer range, just
                // update the pointer.
                if((offset >= 0 && offset < off_type(egptr()-gptr())) ||
                   (offset < 0 && -offset <= off_type(gptr()-eback())))
                {
                    // Get the current file offset to report the correct read
                    // offset.
                    fpos.QuadPart = 0;
                    if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_CURRENT))
                        return traits_type::eof();
                    setg(eback(), gptr()+offset, egptr());
                    return fpos.QuadPart - off_type(egptr()-gptr());
                }
                // Need to offset for the file offset being at egptr() while
                // the requested offset is relative to gptr().
                offset -= off_type(egptr()-gptr());
                fpos.QuadPart = offset;
                if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_CURRENT))
                    return traits_type::eof();
                break;

            case std::ios_base::end:
                fpos.QuadPart = offset;
                if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_END))
                    return traits_type::eof();
                break;

            default:
                return traits_type::eof();
        }
        setg(0, 0, 0);
        return fpos.QuadPart;
    }

    pos_type seekpos(pos_type pos, std::ios_base::openmode mode) override
    {
        // Simplified version of seekoff
        if(mFile == INVALID_HANDLE_VALUE || (mode&std::ios_base::out) || !(mode&std::ios_base::in))
            return traits_type::eof();

        LARGE_INTEGER fpos;
        fpos.QuadPart = pos;
        if(!SetFilePointerEx(mFile, fpos, &fpos, FILE_BEGIN))
            return traits_type::eof();

        setg(0, 0, 0);
        return fpos.QuadPart;
    }

public:
    bool open(const char *filename)
    {
        alure::Vector<wchar_t> wname;
        int wnamelen;

        wnamelen = MultiByteToWideChar(CP_UTF8, 0, filename, -1, NULL, 0);
        if(wnamelen <= 0) return false;

        wname.resize(wnamelen);
        MultiByteToWideChar(CP_UTF8, 0, filename, -1, wname.data(), wnamelen);

        mFile = CreateFileW(wname.data(), GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(mFile == INVALID_HANDLE_VALUE) return false;
        return true;
    }

    bool is_open() const { return mFile != INVALID_HANDLE_VALUE; }

    StreamBuf() : mFile(INVALID_HANDLE_VALUE)
    { }
    ~StreamBuf() override
    {
        if(mFile != INVALID_HANDLE_VALUE)
            CloseHandle(mFile);
        mFile = INVALID_HANDLE_VALUE;
    }
};

// Inherit from std::istream to use our custom streambuf
class Stream final : public std::istream {
public:
    Stream(const char *filename) : std::istream(new StreamBuf())
    {
        // Set the failbit if the file failed to open.
        if(!(static_cast<StreamBuf*>(rdbuf())->open(filename)))
            clear(failbit);
    }
    ~Stream() override
    { delete rdbuf(); }

    bool is_open() const { return static_cast<StreamBuf*>(rdbuf())->is_open(); }
};
#endif

using DecoderEntryPair = std::pair<alure::String,alure::UniquePtr<alure::DecoderFactory>>;
const DecoderEntryPair sDefaultDecoders[] = {
    { "_alure_int_wave", alure::MakeUnique<alure::WaveDecoderFactory>() },

#ifdef HAVE_VORBISFILE
    { "_alure_int_vorbis", alure::MakeUnique<alure::VorbisFileDecoderFactory>() },
#endif
#ifdef HAVE_LIBFLAC
    { "_alure_int_flac", alure::MakeUnique<alure::FlacDecoderFactory>() },
#endif
#ifdef HAVE_OPUSFILE
    { "_alure_int_opus", alure::MakeUnique<alure::OpusFileDecoderFactory>() },
#endif
#ifdef HAVE_LIBSNDFILE
    { "_alure_int_sndfile", alure::MakeUnique<alure::SndFileDecoderFactory>() },
#endif
#ifdef HAVE_MPG123
    { "_alure_int_mpg123", alure::MakeUnique<alure::Mpg123DecoderFactory>() },
#endif
};
alure::Vector<DecoderEntryPair> sDecoders;


template<typename T>
alure::DecoderOrExceptT GetDecoder(alure::StringView name, alure::UniquePtr<std::istream> &file,
                                   T start, T end)
{
    alure::DecoderOrExceptT ret;
    while(start != end)
    {
        alure::DecoderFactory *factory = start->second.get();
        auto decoder = factory->createDecoder(file);
        if(decoder) return (ret = std::move(decoder));

        if(!file || !(file->clear(),file->seekg(0)))
            return (ret = std::runtime_error("Failed to rewind "+name+" for the next decoder factory"));

        ++start;
    }

    return (ret = nullptr);
}

static alure::DecoderOrExceptT GetDecoder(alure::StringView name, alure::UniquePtr<std::istream> file)
{
    auto decoder = GetDecoder(name, file, sDecoders.begin(), sDecoders.end());
    if(std::holds_alternative<std::runtime_error>(decoder)) return decoder;
    if(std::get<alure::SharedPtr<alure::Decoder>>(decoder)) return decoder;
    decoder = GetDecoder(name, file, std::begin(sDefaultDecoders), std::end(sDefaultDecoders));
    if(std::holds_alternative<std::runtime_error>(decoder)) return decoder;
    if(std::get<alure::SharedPtr<alure::Decoder>>(decoder)) return decoder;
    return (decoder = std::runtime_error("No decoder for "+name));
}

class DefaultFileIOFactory final : public alure::FileIOFactory {
    alure::UniquePtr<std::istream> openFile(const alure::String &name) override
    {
#ifdef _WIN32
        auto file = alure::MakeUnique<Stream>(name.c_str());
#else
        auto file = alure::MakeUnique<std::ifstream>(name.c_str(), std::ios::binary);
#endif
        if(!file->is_open()) file = nullptr;
        return std::move(file);
    }
};
DefaultFileIOFactory sDefaultFileFactory;

alure::UniquePtr<alure::FileIOFactory> sFileFactory;

}

namespace alure
{

Decoder::~Decoder() { }
DecoderFactory::~DecoderFactory() { }

void RegisterDecoder(StringView name, UniquePtr<DecoderFactory> factory)
{
    auto iter = std::lower_bound(sDecoders.begin(), sDecoders.end(), name,
        [](const DecoderEntryPair &entry, StringView rhs) -> bool
        { return entry.first < rhs; }
    );
    if(iter != sDecoders.end())
        throw std::runtime_error("Decoder factory \""+name+"\" already registered");
    sDecoders.insert(iter, std::make_pair(String(name), std::move(factory)));
}

UniquePtr<DecoderFactory> UnregisterDecoder(StringView name)
{
    auto iter = std::lower_bound(sDecoders.begin(), sDecoders.end(), name,
        [](const DecoderEntryPair &entry, StringView rhs) -> bool
        { return entry.first < rhs; }
    );
    if(iter != sDecoders.end())
    {
        UniquePtr<DecoderFactory> factory = std::move(iter->second);
        sDecoders.erase(iter);
        return factory;
    }
    return nullptr;
}


FileIOFactory::~FileIOFactory() { }

UniquePtr<FileIOFactory> FileIOFactory::set(UniquePtr<FileIOFactory> factory)
{
    std::swap(sFileFactory, factory);
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

void MessageHandler::deviceDisconnected(Device)
{
}

void MessageHandler::sourceStopped(Source)
{
}

void MessageHandler::sourceForceStopped(Source)
{
}

void MessageHandler::bufferLoading(StringView, ChannelConfig, SampleType, ALuint, ArrayView<ALbyte>)
{
}

String MessageHandler::resourceNotFound(StringView)
{
    return String();
}


template<typename T>
static inline void LoadALFunc(T **func, const char *name)
{ *func = reinterpret_cast<T*>(alGetProcAddress(name)); }

static void LoadNothing(ContextImpl*) { }

static void LoadEFX(ContextImpl *ctx)
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

static void LoadSourceResampler(ContextImpl *ctx)
{
    LoadALFunc(&ctx->alGetStringiSOFT, "alGetStringiSOFT");
}

static void LoadSourceLatency(ContextImpl *ctx)
{
    LoadALFunc(&ctx->alGetSourcei64vSOFT, "alGetSourcei64vSOFT");
    LoadALFunc(&ctx->alGetSourcedvSOFT, "alGetSourcedvSOFT");
}

static const struct {
    enum ALExtension extension;
    const char name[32];
    void (&loader)(ContextImpl*);
} ALExtensionList[] = {
    { EXT_EFX, "ALC_EXT_EFX", LoadEFX },

    { EXT_FLOAT32,   "AL_EXT_FLOAT32",   LoadNothing },
    { EXT_MCFORMATS, "AL_EXT_MCFORMATS", LoadNothing },
    { EXT_BFORMAT,   "AL_EXT_BFORMAT",   LoadNothing },

    { EXT_MULAW,           "AL_EXT_MULAW",           LoadNothing },
    { EXT_MULAW_MCFORMATS, "AL_EXT_MULAW_MCFORMATS", LoadNothing },
    { EXT_MULAW_BFORMAT,   "AL_EXT_MULAW_BFORMAT",   LoadNothing },

    { SOFT_loop_points,       "AL_SOFT_loop_points",       LoadNothing },
    { SOFT_source_latency,    "AL_SOFT_source_latency",    LoadSourceLatency },
    { SOFT_source_resampler,  "AL_SOFT_source_resampler",  LoadSourceResampler },
    { SOFT_source_spatialize, "AL_SOFT_source_spatialize", LoadNothing },

    { EXT_disconnect, "ALC_EXT_disconnect", LoadNothing },

    { EXT_SOURCE_RADIUS, "AL_EXT_SOURCE_RADIUS", LoadNothing },
    { EXT_STEREO_ANGLES, "AL_EXT_STEREO_ANGLES", LoadNothing },
};


ContextImpl *ContextImpl::sCurrentCtx = nullptr;
thread_local ContextImpl *ContextImpl::sThreadCurrentCtx = nullptr;

void ContextImpl::MakeCurrent(ContextImpl *context)
{
    std::unique_lock<std::mutex> ctxlock(mGlobalCtxMutex);

    if(alcMakeContextCurrent(context ? context->getContext() : nullptr) == ALC_FALSE)
        throw std::runtime_error("Call to alcMakeContextCurrent failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ContextImpl::setupExts), context);
    }
    std::swap(sCurrentCtx, context);
    if(context) context->decRef();

    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = 0;

    if((context = sCurrentCtx) != nullptr)
    {
        ctxlock.unlock();
        context->mWakeThread.notify_all();
    }
}

void ContextImpl::MakeThreadCurrent(ContextImpl *context)
{
    if(!DeviceManagerImpl::SetThreadContext)
        throw std::runtime_error("Thread-local contexts unsupported");
    if(DeviceManagerImpl::SetThreadContext(context ? context->getContext() : nullptr) == ALC_FALSE)
        throw std::runtime_error("Call to alcSetThreadContext failed");
    if(context)
    {
        context->addRef();
        std::call_once(context->mSetExts, std::mem_fn(&ContextImpl::setupExts), context);
    }
    if(sThreadCurrentCtx)
        sThreadCurrentCtx->decRef();
    sThreadCurrentCtx = context;
}

void ContextImpl::setupExts()
{
    ALCdevice *device = mDevice->getDevice();
    std::fill(std::begin(mHasExt), std::end(mHasExt), false);
    for(const auto &entry : ALExtensionList)
    {
        mHasExt[entry.extension] = (strncmp(entry.name, "ALC", 3) == 0) ?
                                   alcIsExtensionPresent(device, entry.name) :
                                   alIsExtensionPresent(entry.name);
        if(mHasExt[entry.extension]) entry.loader(this);
    }
}


void ContextImpl::backgroundProc()
{
    if(DeviceManagerImpl::SetThreadContext && mDevice->hasExtension(EXT_thread_local_context))
        DeviceManagerImpl::SetThreadContext(getContext());

    std::chrono::steady_clock::time_point basetime = std::chrono::steady_clock::now();
    std::chrono::milliseconds waketime(0);
    std::unique_lock<std::mutex> ctxlock(mGlobalCtxMutex);
    while(!mQuitThread.load(std::memory_order_acquire))
    {
        {
            std::lock_guard<std::mutex> srclock(mSourceStreamMutex);
            mStreamingSources.erase(
                std::remove_if(mStreamingSources.begin(), mStreamingSources.end(),
                    [](SourceImpl *source) -> bool
                    { return !source->updateAsync(); }
                ), mStreamingSources.end()
            );
        }

        // Only do one pending buffer at a time. In case there's several large
        // buffers to load, we still need to process streaming sources so they
        // don't underrun.
        PendingBuffer *lastpb = mPendingCurrent.load(std::memory_order_acquire);
        if(PendingBuffer *pb = lastpb->mNext.load(std::memory_order_relaxed))
        {
            pb->mBuffer->load(pb->mFrames, pb->mFormat, std::move(pb->mDecoder), this);
            pb->mPromise.set_value(Buffer(pb->mBuffer));
            Promise<Buffer>().swap(pb->mPromise);
            mPendingCurrent.store(pb, std::memory_order_release);
            continue;
        }

        std::unique_lock<std::mutex> wakelock(mWakeMutex);
        if(!mQuitThread.load(std::memory_order_acquire) && lastpb->mNext.load(std::memory_order_acquire) == nullptr)
        {
            ctxlock.unlock();

            std::chrono::milliseconds interval = mWakeInterval.load(std::memory_order_relaxed);
            if(interval.count() == 0)
                mWakeThread.wait(wakelock);
            else
            {
                auto now = std::chrono::steady_clock::now() - basetime;
                if(now > waketime)
                {
                    auto mult = (now-waketime + interval-std::chrono::milliseconds(1)) / interval;
                    waketime += interval * mult;
                    mWakeThread.wait_until(wakelock, waketime + basetime);
                }
            }
            wakelock.unlock();

            ctxlock.lock();
            while(!mQuitThread.load(std::memory_order_acquire) &&
                  alcGetCurrentContext() != getContext())
                mWakeThread.wait(ctxlock);
        }
    }
    ctxlock.unlock();

    if(DeviceManagerImpl::SetThreadContext)
        DeviceManagerImpl::SetThreadContext(nullptr);
}


ContextImpl::ContextImpl(ALCcontext *context, DeviceImpl *device)
  : mListener(this), mContext(context), mDevice(device),
    mWakeInterval(std::chrono::milliseconds::zero()), mQuitThread(false),
    mRefs(0), mHasExt{false}, mIsConnected(true), mIsBatching(false),
    alGetSourcei64vSOFT(0), alGetSourcedvSOFT(0),
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
    mPendingHead = new PendingBuffer;
    mPendingCurrent.store(mPendingHead, std::memory_order_relaxed);
    mPendingTail = mPendingHead;
}

ContextImpl::~ContextImpl()
{
    PendingBuffer *pb = mPendingTail;
    while(pb)
    {
        PendingBuffer *next = pb->mNext.load(std::memory_order_relaxed);
        delete pb;
        pb = next;
    }
    mPendingCurrent.store(nullptr, std::memory_order_relaxed);
    mPendingHead = nullptr;
    mPendingTail = nullptr;
}


void ContextImpl::destroy()
{
    if(mRefs.load() != 0)
        throw std::runtime_error("Context is in use");
    if(!mBuffers.empty())
        throw std::runtime_error("Trying to destroy a context with buffers");

    if(mThread.joinable())
    {
        std::unique_lock<std::mutex> lock(mWakeMutex);
        mQuitThread.store(true, std::memory_order_release);
        lock.unlock();
        mWakeThread.notify_all();
        mThread.join();
    }

    alcDestroyContext(mContext);
    mContext = nullptr;

    mDevice->removeContext(this);
}


void ContextImpl::startBatch()
{
    alcSuspendContext(mContext);
    mIsBatching = true;
}

void ContextImpl::endBatch()
{
    alcProcessContext(mContext);
    mIsBatching = false;
}


SharedPtr<MessageHandler> ContextImpl::setMessageHandler(SharedPtr<MessageHandler> handler)
{
    std::lock_guard<std::mutex> lock(mGlobalCtxMutex);
    mMessage.swap(handler);
    return handler;
}


void ContextImpl::setAsyncWakeInterval(std::chrono::milliseconds interval)
{
    if(interval.count() < 0 || interval > std::chrono::seconds(1))
        throw std::runtime_error("Async wake interval out of range");
    mWakeInterval.store(interval);
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();
}


DecoderOrExceptT ContextImpl::findDecoder(StringView name)
{
    DecoderOrExceptT ret;

    String oldname = String(name);
    auto file = FileIOFactory::get().openFile(oldname);
    if(file) return (ret = GetDecoder(name, std::move(file)));

    // Resource not found. Try to find a substitute.
    if(!mMessage.get()) return (ret = std::runtime_error("Failed to open "+oldname));
    do {
        String newname(mMessage->resourceNotFound(oldname));
        if(newname.empty())
            return (ret = std::runtime_error("Failed to open "+oldname));
        file = FileIOFactory::get().openFile(newname);
        oldname = std::move(newname);
    } while(!file);

    return (ret = GetDecoder(oldname, std::move(file)));
}

SharedPtr<Decoder> ContextImpl::createDecoder(StringView name)
{
    CheckContext(this);
    DecoderOrExceptT dec = findDecoder(name);
    if(SharedPtr<Decoder> *decoder = std::get_if<SharedPtr<Decoder>>(&dec))
        return *decoder;
    throw std::get<std::runtime_error>(dec);
}


bool ContextImpl::isSupported(ChannelConfig channels, SampleType type) const
{
    CheckContext(this);
    return GetFormat(channels, type) != AL_NONE;
}


ArrayView<String> ContextImpl::getAvailableResamplers()
{
    CheckContext(this);
    if(mResamplers.empty() && hasExtension(SOFT_source_resampler))
    {
        ALint num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
        mResamplers.reserve(num_resamplers);
        for(int i = 0;i < num_resamplers;i++)
            mResamplers.emplace_back(alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i));
        if(mResamplers.empty())
            mResamplers.emplace_back();
    }
    return mResamplers;
}

ALsizei ContextImpl::getDefaultResamplerIndex() const
{
    CheckContext(this);
    if(!hasExtension(SOFT_source_resampler))
        return 0;
    return alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);
}


BufferOrExceptT ContextImpl::doCreateBuffer(StringView name, Vector<UniquePtr<BufferImpl>>::iterator iter, SharedPtr<Decoder> decoder)
{
    BufferOrExceptT retval;
    ALuint srate = decoder->getFrequency();
    ChannelConfig chans = decoder->getChannelConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();

    Vector<ALbyte> data(FramesToBytes(frames, chans, type));
    frames = decoder->read(data.data(), frames);
    if(!frames) return (retval = std::runtime_error("No samples for buffer"));
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
    if(format == AL_NONE)
    {
        std::stringstream sstr;
        sstr<< "Format not supported ("<<GetSampleTypeName(type)<<", "<<GetChannelConfigName(chans)<<")";
        return (retval = std::runtime_error(sstr.str()));
    }

    if(mMessage.get())
        mMessage->bufferLoading(name, chans, type, srate, data);

    alGetError();
    ALuint bid = 0;
    alGenBuffers(1, &bid);
    alBufferData(bid, format, data.data(), data.size(), srate);
    if(hasExtension(SOFT_loop_points))
    {
        ALint pts[2]{(ALint)loop_pts.first, (ALint)loop_pts.second};
        alBufferiv(bid, AL_LOOP_POINTS_SOFT, pts);
    }
    if(alGetError() != AL_NO_ERROR)
    {
        alDeleteBuffers(1, &bid);
        return (retval = std::runtime_error("Failed to buffer data"));
    }

    return (retval = mBuffers.insert(iter,
        MakeUnique<BufferImpl>(this, bid, srate, chans, type, name)
    )->get());
}

BufferOrExceptT ContextImpl::doCreateBufferAsync(StringView name, Vector<UniquePtr<BufferImpl>>::iterator iter, SharedPtr<Decoder> decoder, Promise<Buffer> promise)
{
    BufferOrExceptT retval;
    ALuint srate = decoder->getFrequency();
    ChannelConfig chans = decoder->getChannelConfig();
    SampleType type = decoder->getSampleType();
    ALuint frames = decoder->getLength();
    if(!frames) return (retval = std::runtime_error("No samples for buffer"));

    ALenum format = GetFormat(chans, type);
    if(format == AL_NONE)
    {
        std::stringstream sstr;
        sstr<< "Format not supported ("<<GetSampleTypeName(type)<<", "<<GetChannelConfigName(chans)<<")";
        return (retval = std::runtime_error(sstr.str()));
    }

    alGetError();
    ALuint bid = 0;
    alGenBuffers(1, &bid);
    if(alGetError() != AL_NO_ERROR)
        return (retval = std::runtime_error("Failed to create buffer"));

    auto buffer = MakeUnique<BufferImpl>(this, bid, srate, chans, type, name);

    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ContextImpl::backgroundProc), this);

    PendingBuffer *pb = nullptr;
    if(mPendingTail == mPendingCurrent.load(std::memory_order_acquire))
        pb = new PendingBuffer{buffer.get(), decoder, format, frames, std::move(promise)};
    else
    {
        pb = mPendingTail;
        pb->mBuffer = buffer.get();
        pb->mDecoder = decoder;
        pb->mFormat = format;
        pb->mFrames = frames;
        pb->mPromise = std::move(promise);
        mPendingTail = pb->mNext.exchange(nullptr, std::memory_order_relaxed);
    }

    mPendingHead->mNext.store(pb, std::memory_order_release);
    mPendingHead = pb;

    return (retval = mBuffers.insert(iter, std::move(buffer))->get());
}

Buffer ContextImpl::getBuffer(StringView name)
{
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    if(Expect<false>(!mFutureBuffers.empty()))
    {
        Buffer buffer;

        // If the buffer is already pending for the future, wait for it
        auto iter = std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), hasher(name),
            [hasher](const PendingFuture &lhs, size_t rhs) -> bool
            { return hasher(lhs.mBuffer->getName()) < rhs; }
        );
        if(iter != mFutureBuffers.end() && iter->mBuffer->getName() == name)
        {
            buffer = iter->mFuture.get();
            mFutureBuffers.erase(iter);
        }

        // Clear out any completed futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingFuture &entry) -> bool
                { return entry.mFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );

        // If we got the buffer, return it. Otherwise, go load it normally.
        if(buffer) return buffer;
    }

    auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), hasher(name),
        [hasher](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mBuffers.end() && (*iter)->getName() == name)
        return Buffer(iter->get());

    BufferOrExceptT ret = doCreateBuffer(name, iter, createDecoder(name));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(Expect<false>(!buffer))
        throw std::get<std::runtime_error>(ret);
    return *buffer;
}

SharedFuture<Buffer> ContextImpl::getBufferAsync(StringView name)
{
    SharedFuture<Buffer> future;
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    if(Expect<false>(!mFutureBuffers.empty()))
    {
        // Check if the future that's being created already exists
        auto iter = std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), hasher(name),
            [hasher](const PendingFuture &lhs, size_t rhs) -> bool
            { return hasher(lhs.mBuffer->getName()) < rhs; }
        );
        if(iter != mFutureBuffers.end() && iter->mBuffer->getName() == name)
        {
            future = iter->mFuture;
            if(future.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready)
                mFutureBuffers.erase(iter);
            return future;
        }

        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingFuture &entry) -> bool
                { return entry.mFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), hasher(name),
        [hasher](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mBuffers.end() && (*iter)->getName() == name)
    {
        // User asked to create a future buffer that's already loaded. Just
        // construct a promise, fulfill the promise immediately, then return a
        // shared future that's already set.
        Promise<Buffer> promise;
        promise.set_value(Buffer(iter->get()));
        return promise.get_future().share();
    }

    Promise<Buffer> promise;
    future = promise.get_future().share();

    BufferOrExceptT ret = doCreateBufferAsync(name, iter, createDecoder(name), std::move(promise));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(Expect<false>(!buffer))
        throw std::get<std::runtime_error>(ret);
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();

    mFutureBuffers.insert(
        std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), hasher(name),
            [hasher](const PendingFuture &lhs, size_t rhs) -> bool
            { return hasher(lhs.mBuffer->getName()) < rhs; }
        ), { buffer->getHandle(), future }
    );

    return future;
}

void ContextImpl::precacheBuffersAsync(ArrayView<StringView> names)
{
    CheckContext(this);

    if(Expect<false>(!mFutureBuffers.empty()))
    {
        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingFuture &entry) -> bool
                { return entry.mFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto hasher = std::hash<StringView>();
    for(const StringView name : names)
    {
        // Check if the buffer that's being created already exists
        auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), hasher(name),
            [hasher](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
            { return hasher(lhs->getName()) < rhs; }
        );
        if(iter != mBuffers.end() && (*iter)->getName() == name)
            continue;

        DecoderOrExceptT dec = findDecoder(name);
        SharedPtr<Decoder> *decoder = std::get_if<SharedPtr<Decoder>>(&dec);
        if(!decoder) continue;

        Promise<Buffer> promise;
        SharedFuture<Buffer> future = promise.get_future().share();

        BufferOrExceptT buf = doCreateBufferAsync(name, iter, std::move(*decoder),
                                                  std::move(promise));
        Buffer *buffer = std::get_if<Buffer>(&buf);
        if(Expect<false>(!buffer)) continue;

        mFutureBuffers.insert(
            std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), hasher(name),
                [hasher](const PendingFuture &lhs, size_t rhs) -> bool
                { return hasher(lhs.mBuffer->getName()) < rhs; }
            ), { buffer->getHandle(), future }
        );
    }
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();
}

Buffer ContextImpl::createBufferFrom(StringView name, SharedPtr<Decoder> decoder)
{
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), hasher(name),
        [hasher](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mBuffers.end() && (*iter)->getName() == name)
        throw std::runtime_error("Buffer \""+name+"\" already exists");

    BufferOrExceptT ret = doCreateBuffer(name, iter, std::move(decoder));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(Expect<false>(!buffer))
        throw std::get<std::runtime_error>(ret);
    return *buffer;
}

SharedFuture<Buffer> ContextImpl::createBufferAsyncFrom(StringView name, SharedPtr<Decoder> decoder)
{
    SharedFuture<Buffer> future;
    CheckContext(this);

    if(Expect<false>(!mFutureBuffers.empty()))
    {
        // Clear out any fulfilled futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingFuture &entry) -> bool
                { return entry.mFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto hasher = std::hash<StringView>();
    auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), hasher(name),
        [hasher](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mBuffers.end() && (*iter)->getName() == name)
        throw std::runtime_error("Buffer \""+name+"\" already exists");

    Promise<Buffer> promise;
    future = promise.get_future().share();

    BufferOrExceptT ret = doCreateBufferAsync(name, iter, std::move(decoder), std::move(promise));
    Buffer *buffer = std::get_if<Buffer>(&ret);
    if(Expect<false>(!buffer))
        throw std::get<std::runtime_error>(ret);
    mWakeMutex.lock(); mWakeMutex.unlock();
    mWakeThread.notify_all();

    mFutureBuffers.insert(
        std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), hasher(name),
            [hasher](const PendingFuture &lhs, size_t rhs) -> bool
            { return hasher(lhs.mBuffer->getName()) < rhs; }
        ), { buffer->getHandle(), future }
    );

    return future;
}


void ContextImpl::removeBuffer(StringView name)
{
    CheckContext(this);

    auto hasher = std::hash<StringView>();
    if(Expect<false>(!mFutureBuffers.empty()))
    {
        // If the buffer is already pending for the future, wait for it to
        // finish before continuing.
        auto iter = std::lower_bound(mFutureBuffers.begin(), mFutureBuffers.end(), hasher(name),
            [hasher](const PendingFuture &lhs, size_t rhs) -> bool
            { return hasher(lhs.mBuffer->getName()) < rhs; }
        );
        if(iter != mFutureBuffers.end() && iter->mBuffer->getName() == name)
        {
            iter->mFuture.wait();
            mFutureBuffers.erase(iter);
        }

        // Clear out any completed futures.
        mFutureBuffers.erase(
            std::remove_if(mFutureBuffers.begin(), mFutureBuffers.end(),
                [](const PendingFuture &entry) -> bool
                { return entry.mFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
            ), mFutureBuffers.end()
        );
    }

    auto iter = std::lower_bound(mBuffers.begin(), mBuffers.end(), hasher(name),
        [hasher](const UniquePtr<BufferImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mBuffers.end() && (*iter)->getName() == name)
    {
        // Remove pending sources whose future was waiting for this buffer.
        mPendingSources.erase(
            std::remove_if(mPendingSources.begin(), mPendingSources.end(),
                [iter](PendingSource &entry) -> bool
                {
                    return (entry.mFuture.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready &&
                            entry.mFuture.get().getHandle() == iter->get());
                }
            ), mPendingSources.end()
        );
        (*iter)->cleanup();
        mBuffers.erase(iter);
    }
}


ALuint ContextImpl::getSourceId(ALuint maxprio)
{
    ALuint id = 0;
    if(mSourceIds.empty())
    {
        alGetError();
        alGenSources(1, &id);
        if(alGetError() == AL_NO_ERROR)
            return id;

        SourceImpl *lowest = nullptr;
        for(SourceBufferUpdateEntry &entry : mPlaySources)
        {
            if(!lowest || entry.mSource->getPriority() < lowest->getPriority())
                lowest = entry.mSource;
        }
        for(SourceStreamUpdateEntry &entry : mStreamSources)
        {
            if(!lowest || entry.mSource->getPriority() < lowest->getPriority())
                lowest = entry.mSource;
        }
        if(lowest && lowest->getPriority() < maxprio)
        {
            lowest->stop();
            if(mMessage.get())
                mMessage->sourceForceStopped(lowest);
        }
    }
    if(mSourceIds.empty())
        throw std::runtime_error("No available sources");

    id = mSourceIds.top();
    mSourceIds.pop();
    return id;
}


Source ContextImpl::createSource()
{
    CheckContext(this);

    SourceImpl *source;
    if(!mFreeSources.empty())
    {
        source = mFreeSources.back();
        mFreeSources.pop_back();
    }
    else
    {
        mAllSources.emplace_back(this);
        source = &mAllSources.back();
    }
    return Source(source);
}


void ContextImpl::addPendingSource(SourceImpl *source, SharedFuture<Buffer> future)
{
    auto iter = std::lower_bound(mPendingSources.begin(), mPendingSources.end(), source,
        [](const PendingSource &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter == mPendingSources.end() || iter->mSource != source)
        mPendingSources.insert(iter, {source, std::move(future)});
}

void ContextImpl::removePendingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mPendingSources.begin(), mPendingSources.end(), source,
        [](const PendingSource &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter != mPendingSources.end() && iter->mSource == source)
        mPendingSources.erase(iter);
}

bool ContextImpl::isPendingSource(const SourceImpl *source) const
{
    auto iter = std::lower_bound(mPendingSources.begin(), mPendingSources.end(), source,
        [](const PendingSource &lhs, const SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    return (iter != mPendingSources.end() && iter->mSource == source);
}

void ContextImpl::addFadingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mFadingSources.begin(), mFadingSources.end(), source,
        [](SourceImpl *lhs, SourceImpl *rhs) -> bool
        { return lhs < rhs; }
    );
    if(iter == mFadingSources.end() || *iter != source)
        mFadingSources.insert(iter, source);
}

void ContextImpl::removeFadingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mFadingSources.begin(), mFadingSources.end(), source,
        [](SourceImpl *lhs, SourceImpl *rhs) -> bool
        { return lhs < rhs; }
    );
    if(iter != mFadingSources.end() && *iter == source)
        mFadingSources.erase(iter);
}

void ContextImpl::addPlayingSource(SourceImpl *source, ALuint id)
{
    auto iter = std::lower_bound(mPlaySources.begin(), mPlaySources.end(), source,
        [](const SourceBufferUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter == mPlaySources.end() || iter->mSource != source)
        mPlaySources.insert(iter, {source,id});
}

void ContextImpl::addPlayingSource(SourceImpl *source)
{
    auto iter = std::lower_bound(mStreamSources.begin(), mStreamSources.end(), source,
        [](const SourceStreamUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter == mStreamSources.end() || iter->mSource != source)
        mStreamSources.insert(iter, {source});
}

void ContextImpl::removePlayingSource(SourceImpl *source)
{
    auto iter0 = std::lower_bound(mPlaySources.begin(), mPlaySources.end(), source,
        [](const SourceBufferUpdateEntry &lhs, SourceImpl *rhs) -> bool
        { return lhs.mSource < rhs; }
    );
    if(iter0 != mPlaySources.end() && iter0->mSource == source)
        mPlaySources.erase(iter0);
    else
    {
        auto iter1 = std::lower_bound(mStreamSources.begin(), mStreamSources.end(), source,
            [](const SourceStreamUpdateEntry &lhs, SourceImpl *rhs) -> bool
            { return lhs.mSource < rhs; }
        );
        if(iter1 != mStreamSources.end() && iter1->mSource == source)
            mStreamSources.erase(iter1);
    }
}


void ContextImpl::addStream(SourceImpl *source)
{
    std::lock_guard<std::mutex> lock(mSourceStreamMutex);
    if(mThread.get_id() == std::thread::id())
        mThread = std::thread(std::mem_fn(&ContextImpl::backgroundProc), this);
    auto iter = std::lower_bound(mStreamingSources.begin(), mStreamingSources.end(), source);
    if(iter == mStreamingSources.end() || *iter != source)
        mStreamingSources.insert(iter, source);
}

void ContextImpl::removeStream(SourceImpl *source)
{
    std::lock_guard<std::mutex> lock(mSourceStreamMutex);
    auto iter = std::lower_bound(mStreamingSources.begin(), mStreamingSources.end(), source);
    if(iter != mStreamingSources.end() && *iter == source)
        mStreamingSources.erase(iter);
}

void ContextImpl::removeStreamNoLock(SourceImpl *source)
{
    auto iter = std::lower_bound(mStreamingSources.begin(), mStreamingSources.end(), source);
    if(iter != mStreamingSources.end() && *iter == source)
        mStreamingSources.erase(iter);
}


AuxiliaryEffectSlot ContextImpl::createAuxiliaryEffectSlot()
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
        return AuxiliaryEffectSlot(new AuxiliaryEffectSlotImpl(this, id));
    }
    catch(...) {
        alDeleteAuxiliaryEffectSlots(1, &id);
        throw;
    }
}


Effect ContextImpl::createEffect()
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
        return Effect(new EffectImpl(this, id));
    }
    catch(...) {
        alDeleteEffects(1, &id);
        throw;
    }
}


SourceGroup ContextImpl::createSourceGroup(StringView name)
{
    auto hasher = std::hash<StringView>();
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), hasher(name),
        [hasher](const UniquePtr<SourceGroupImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mSourceGroups.end() && (*iter)->getName() == name)
        throw std::runtime_error("Duplicate source group name");
    iter = mSourceGroups.insert(iter, MakeUnique<SourceGroupImpl>(this, String(name)));
    return SourceGroup(iter->get());
}

SourceGroup ContextImpl::getSourceGroup(StringView name)
{
    auto hasher = std::hash<StringView>();
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), hasher(name),
        [hasher](const UniquePtr<SourceGroupImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter == mSourceGroups.end() || (*iter)->getName() != name)
        throw std::runtime_error("Source group not found");
    return SourceGroup(iter->get());
}

void ContextImpl::freeSourceGroup(SourceGroupImpl *group)
{
    auto hasher = std::hash<StringView>();
    auto iter = std::lower_bound(mSourceGroups.begin(), mSourceGroups.end(), hasher(group->getName()),
        [hasher](const UniquePtr<SourceGroupImpl> &lhs, size_t rhs) -> bool
        { return hasher(lhs->getName()) < rhs; }
    );
    if(iter != mSourceGroups.end() && iter->get() == group)
        mSourceGroups.erase(iter);
}


void ContextImpl::setDopplerFactor(ALfloat factor)
{
    if(!(factor >= 0.0f))
        throw std::runtime_error("Doppler factor out of range");
    CheckContext(this);
    alDopplerFactor(factor);
}


void ContextImpl::setSpeedOfSound(ALfloat speed)
{
    if(!(speed > 0.0f))
        throw std::runtime_error("Speed of sound out of range");
    CheckContext(this);
    alSpeedOfSound(speed);
}


void ContextImpl::setDistanceModel(DistanceModel model)
{
    CheckContext(this);
    alDistanceModel((ALenum)model);
}


void ContextImpl::update()
{
    CheckContext(this);
    mPendingSources.erase(
        std::remove_if(mPendingSources.begin(), mPendingSources.end(),
            [](PendingSource &entry) -> bool
            { return !entry.mSource->checkPending(entry.mFuture); }
        ), mPendingSources.end()
    );
    if(!mFadingSources.empty())
    {
        auto cur_time = std::chrono::steady_clock::now();
        mFadingSources.erase(
            std::remove_if(mFadingSources.begin(), mFadingSources.end(),
                [cur_time](SourceImpl *source) -> bool
                { return !source->fadeUpdate(cur_time); }
            ), mFadingSources.end()
        );
    }
    mPlaySources.erase(
        std::remove_if(mPlaySources.begin(), mPlaySources.end(),
            [](const SourceBufferUpdateEntry &entry) -> bool
            { return !entry.mSource->playUpdate(entry.mId); }
        ), mPlaySources.end()
    );
    mStreamSources.erase(
        std::remove_if(mStreamSources.begin(), mStreamSources.end(),
            [](const SourceStreamUpdateEntry &entry) -> bool
            { return !entry.mSource->playUpdate(); }
        ), mStreamSources.end()
    );

    if(!mWakeInterval.load(std::memory_order_relaxed).count())
    {
        // For performance reasons, don't wait for the thread's mutex. This
        // should be called often enough to keep up with any and all streams
        // regardless.
        mWakeThread.notify_all();
    }

    if(hasExtension(EXT_disconnect) && mIsConnected)
    {
        ALCint connected;
        alcGetIntegerv(alcGetContextsDevice(mContext), ALC_CONNECTED, 1, &connected);
        mIsConnected = connected;
        if(!connected && mMessage.get()) mMessage->deviceDisconnected(Device(mDevice));
    }
}

void Context::destroy()
{
    pImpl->destroy();
    pImpl = nullptr;
}
DECL_THUNK0(Device, Context, getDevice,)
DECL_THUNK0(void, Context, startBatch,)
DECL_THUNK0(void, Context, endBatch,)
DECL_THUNK0(Listener, Context, getListener,)
DECL_THUNK1(SharedPtr<MessageHandler>, Context, setMessageHandler,, SharedPtr<MessageHandler>)
DECL_THUNK0(SharedPtr<MessageHandler>, Context, getMessageHandler, const)
DECL_THUNK1(void, Context, setAsyncWakeInterval,, std::chrono::milliseconds)
DECL_THUNK0(std::chrono::milliseconds, Context, getAsyncWakeInterval, const)
DECL_THUNK1(SharedPtr<Decoder>, Context, createDecoder,, StringView)
DECL_THUNK2(bool, Context, isSupported, const, ChannelConfig, SampleType)
DECL_THUNK0(ArrayView<String>, Context, getAvailableResamplers,)
DECL_THUNK0(ALsizei, Context, getDefaultResamplerIndex, const)
DECL_THUNK1(Buffer, Context, getBuffer,, StringView)
DECL_THUNK1(SharedFuture<Buffer>, Context, getBufferAsync,, StringView)
DECL_THUNK1(void, Context, precacheBuffersAsync,, ArrayView<StringView>)
DECL_THUNK2(Buffer, Context, createBufferFrom,, StringView, SharedPtr<Decoder>)
DECL_THUNK2(SharedFuture<Buffer>, Context, createBufferAsyncFrom,, StringView, SharedPtr<Decoder>)
DECL_THUNK1(void, Context, removeBuffer,, StringView)
DECL_THUNK1(void, Context, removeBuffer,, Buffer)
DECL_THUNK0(Source, Context, createSource,)
DECL_THUNK0(AuxiliaryEffectSlot, Context, createAuxiliaryEffectSlot,)
DECL_THUNK0(Effect, Context, createEffect,)
DECL_THUNK1(SourceGroup, Context, createSourceGroup,, StringView)
DECL_THUNK1(SourceGroup, Context, getSourceGroup,, StringView)
DECL_THUNK1(void, Context, setDopplerFactor,, ALfloat)
DECL_THUNK1(void, Context, setSpeedOfSound,, ALfloat)
DECL_THUNK1(void, Context, setDistanceModel,, DistanceModel)
DECL_THUNK0(void, Context, update,)


void Context::MakeCurrent(Context context)
{ ContextImpl::MakeCurrent(context.pImpl); }

Context Context::GetCurrent()
{ return Context(ContextImpl::GetCurrent()); }

void Context::MakeThreadCurrent(Context context)
{ ContextImpl::MakeThreadCurrent(context.pImpl); }

Context Context::GetThreadCurrent()
{ return Context(ContextImpl::GetThreadCurrent()); }


void ListenerImpl::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    alListenerf(AL_GAIN, gain);
}


void ListenerImpl::set3DParameters(const Vector3 &position, const Vector3 &velocity, std::pair<Vector3,Vector3> orientation)
{
    static_assert(sizeof(orientation) == sizeof(ALfloat[6]), "Invalid Vector3 pair size");
    CheckContext(mContext);
    Batcher batcher = mContext->getBatcher();
    alListenerfv(AL_POSITION, position.getPtr());
    alListenerfv(AL_VELOCITY, velocity.getPtr());
    alListenerfv(AL_ORIENTATION, orientation.first.getPtr());
}

void ListenerImpl::setPosition(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    alListener3f(AL_POSITION, x, y, z);
}

void ListenerImpl::setPosition(const ALfloat *pos)
{
    CheckContext(mContext);
    alListenerfv(AL_POSITION, pos);
}

void ListenerImpl::setVelocity(ALfloat x, ALfloat y, ALfloat z)
{
    CheckContext(mContext);
    alListener3f(AL_VELOCITY, x, y, z);
}

void ListenerImpl::setVelocity(const ALfloat *vel)
{
    CheckContext(mContext);
    alListenerfv(AL_VELOCITY, vel);
}

void ListenerImpl::setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2)
{
    CheckContext(mContext);
    ALfloat ori[6] = { x1, y1, z1, x2, y2, z2 };
    alListenerfv(AL_ORIENTATION, ori);
}

void ListenerImpl::setOrientation(const ALfloat *at, const ALfloat *up)
{
    CheckContext(mContext);
    ALfloat ori[6] = { at[0], at[1], at[2], up[0], up[1], up[2] };
    alListenerfv(AL_ORIENTATION, ori);
}

void ListenerImpl::setOrientation(const ALfloat *ori)
{
    CheckContext(mContext);
    alListenerfv(AL_ORIENTATION, ori);
}

void ListenerImpl::setMetersPerUnit(ALfloat m_u)
{
    if(!(m_u > 0.0f))
        throw std::runtime_error("Invalid meters per unit");
    CheckContext(mContext);
    if(mContext->hasExtension(EXT_EFX))
        alListenerf(AL_METERS_PER_UNIT, m_u);
}


using Vector3Pair = std::pair<Vector3,Vector3>;

DECL_THUNK1(void, Listener, setGain,, ALfloat)
DECL_THUNK3(void, Listener, set3DParameters,, const Vector3&, const Vector3&, Vector3Pair)
DECL_THUNK3(void, Listener, setPosition,, ALfloat, ALfloat, ALfloat)
DECL_THUNK1(void, Listener, setPosition,, const ALfloat*)
DECL_THUNK3(void, Listener, setVelocity,, ALfloat, ALfloat, ALfloat)
DECL_THUNK1(void, Listener, setVelocity,, const ALfloat*)
DECL_THUNK6(void, Listener, setOrientation,, ALfloat, ALfloat, ALfloat, ALfloat, ALfloat, ALfloat)
DECL_THUNK2(void, Listener, setOrientation,, const ALfloat*, const ALfloat*)
DECL_THUNK1(void, Listener, setOrientation,, const ALfloat*)
DECL_THUNK1(void, Listener, setMetersPerUnit,, ALfloat)

}
