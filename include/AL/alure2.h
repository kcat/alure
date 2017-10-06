#ifndef AL_ALURE2_H
#define AL_ALURE2_H

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <chrono>
#include <array>
#include <cmath>

#include "alc.h"
#include "al.h"

#ifdef _WIN32
#if defined(ALURE_BUILD_STATIC) || defined(ALURE_STATIC_LIB)
#define ALURE_API
#elif defined(ALURE_BUILD_DLL)
#define ALURE_API __declspec(dllexport)
#else
#define ALURE_API __declspec(dllimport)
#endif

#else

#define ALURE_API
#endif

#ifndef EFXEAXREVERBPROPERTIES_DEFINED
#define EFXEAXREVERBPROPERTIES_DEFINED
typedef struct {
    float flDensity;
    float flDiffusion;
    float flGain;
    float flGainHF;
    float flGainLF;
    float flDecayTime;
    float flDecayHFRatio;
    float flDecayLFRatio;
    float flReflectionsGain;
    float flReflectionsDelay;
    float flReflectionsPan[3];
    float flLateReverbGain;
    float flLateReverbDelay;
    float flLateReverbPan[3];
    float flEchoTime;
    float flEchoDepth;
    float flModulationTime;
    float flModulationDepth;
    float flAirAbsorptionGainHF;
    float flHFReference;
    float flLFReference;
    float flRoomRolloffFactor;
    int   iDecayHFLimit;
} EFXEAXREVERBPROPERTIES, *LPEFXEAXREVERBPROPERTIES;
#endif

namespace alure {

class DeviceManager;
class Device;
class ALDevice;
class Context;
class ALContext;
class Listener;
class ALListener;
class Buffer;
class ALBuffer;
class Source;
class ALSource;
class SourceGroup;
class ALSourceGroup;
class AuxiliaryEffectSlot;
class ALAuxiliaryEffectSlot;
class Effect;
class ALEffect;
class Decoder;
class DecoderFactory;
class MessageHandler;


// Duration in seconds, using double precision
using Seconds = std::chrono::duration<double>;

// A SharedPtr implementation, defaults to C++11's std::shared_ptr. If this is
// changed, you must recompile the library.
template<typename T>
using SharedPtr = std::shared_ptr<T>;
template<typename T, typename... Args>
constexpr inline SharedPtr<T> MakeShared(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// A UniquePtr implementation, defaults to C++11's std::unique_ptr. If this is
// changed, you must recompile the library.
template<typename T, typename D = std::default_delete<T>>
using UniquePtr = std::unique_ptr<T, D>;
template<typename T, typename... Args>
constexpr inline UniquePtr<T> MakeUnique(Args&&... args)
{
#if __cplusplus >= 201402L
    return std::make_unique<T>(std::forward<Args>(args)...);
#else
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
#endif
}

// A Vector implementation, defaults to C++'s std::vector. If this is changed,
// you must recompile the library.
template<typename T>
using Vector = std::vector<T>;

// A String implementation, default's to C++'s std::string. If this is changed,
// you must recompile the library.
using String = std::string;

// A rather simple ArrayView container. This allows accepting various array
// types (std::array, Vector, a static-sized array, a dynamic array + size)
// without copying its elements.
template<typename T>
class ArrayView {
    T *mElems;
    size_t mNumElems;

public:
    typedef T *iterator;
    typedef const T *const_iterator;

    ArrayView() : mElems(nullptr), mNumElems(0) { }
    ArrayView(const ArrayView &rhs) : mElems(rhs.data()), mNumElems(rhs.size()) { }
    ArrayView(ArrayView&& rhs) : mElems(rhs.data()), mNumElems(rhs.size()) { }
    ArrayView(T *elems, size_t num_elems) : mElems(elems), mNumElems(num_elems) { }
    template<size_t N>
    ArrayView(T (&elems)[N]) : mElems(elems), mNumElems(N) { }
    template<typename OtherT>
    ArrayView(OtherT &arr) : mElems(arr.data()), mNumElems(arr.size()) { }

    ArrayView& operator=(const ArrayView &rhs)
    {
        mElems = rhs.data();
        mNumElems = rhs.size();
    }
    ArrayView& operator=(ArrayView&& rhs)
    {
        mElems = rhs.data();
        mNumElems = rhs.size();
    }
    template<size_t N>
    ArrayView& operator=(T (&elems)[N])
    {
        mElems = elems;
        mNumElems = N;
    }
    template<typename OtherT>
    ArrayView& operator=(OtherT &arr)
    {
        mElems = arr.data();
        mNumElems = arr.size();
    }


    const T *data() const { return mElems; }
    T *data() { return mElems; }

    size_t size() const { return mNumElems; }
    bool empty() const { return mNumElems == 0; }

    const T& operator[](size_t i) const { return mElems[i]; }
    T& operator[](size_t i) { return mElems[i]; }

    const T& front() const { return mElems[0]; }
    T& front() { return mElems[0]; }
    const T& back() const { return mElems[mNumElems-1]; }
    T& back() { return mElems[mNumElems-1]; }

    const T& at(size_t i) const
    {
        if(i >= mNumElems)
            throw std::out_of_range("alure::ArrayView::at: element out of range");
        return mElems[i];
    }
    T& at(size_t i)
    {
        if(i >= mNumElems)
            throw std::out_of_range("alure::ArrayView::at: element out of range");
        return mElems[i];
    }

    iterator begin() { return mElems; }
    const_iterator begin() const { return mElems; }
    const_iterator cbegin() const { return mElems; }

    iterator end() { return mElems + mNumElems; }
    const_iterator end() const { return mElems + mNumElems; }
    const_iterator cend() const { return mElems + mNumElems; }
};


/**
 * An attribute pair, for passing attributes to Device::createContext and
 * Device::reset.
 */
using AttributePair = std::pair<ALCint,ALCint>;
static_assert(sizeof(AttributePair) == sizeof(ALCint[2]), "Bad AttributePair size");


struct FilterParams {
    ALfloat mGain;
    ALfloat mGainHF; // For low-pass and band-pass filters
    ALfloat mGainLF; // For high-pass and band-pass filters
};


class ALURE_API Vector3 {
    std::array<ALfloat,3> mValue;

public:
    constexpr Vector3() noexcept
      : mValue{{0.0f, 0.0f, 0.0f}}
    { }
    constexpr Vector3(const Vector3 &rhs) noexcept
      : mValue{{rhs.mValue[0], rhs.mValue[1], rhs.mValue[2]}}
    { }
    constexpr Vector3(ALfloat val) noexcept
      : mValue{{val, val, val}}
    { }
    constexpr Vector3(ALfloat x, ALfloat y, ALfloat z) noexcept
      : mValue{{x, y, z}}
    { }
    Vector3(const ALfloat *vec) noexcept
      : mValue{{vec[0], vec[1], vec[2]}}
    { }

    const ALfloat *getPtr() const noexcept
    { return mValue.data(); }

    ALfloat& operator[](size_t i) noexcept
    { return mValue[i]; }
    constexpr const ALfloat& operator[](size_t i) const noexcept
    { return mValue[i]; }

#define ALURE_DECL_OP(op)                                            \
    constexpr Vector3 operator op(const Vector3 &rhs) const noexcept \
    {                                                                \
        return Vector3(mValue[0] op rhs.mValue[0],                   \
                       mValue[1] op rhs.mValue[1],                   \
                       mValue[2] op rhs.mValue[2]);                  \
    }
    ALURE_DECL_OP(+)
    ALURE_DECL_OP(-)
    ALURE_DECL_OP(*)
    ALURE_DECL_OP(/)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                             \
    Vector3& operator op(const Vector3 &rhs) noexcept \
    {                                                 \
        mValue[0] op rhs.mValue[0];                   \
        mValue[1] op rhs.mValue[1];                   \
        mValue[2] op rhs.mValue[2];                   \
        return *this;                                 \
    }
    ALURE_DECL_OP(+=)
    ALURE_DECL_OP(-=)
    ALURE_DECL_OP(*=)
    ALURE_DECL_OP(/=)

#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                                       \
    constexpr Vector3 operator op(ALfloat scale) const noexcept \
    {                                                           \
        return Vector3(mValue[0] op scale,                      \
                       mValue[1] op scale,                      \
                       mValue[2] op scale);                     \
    }
    ALURE_DECL_OP(*)
    ALURE_DECL_OP(/)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                        \
    Vector3& operator op(ALfloat scale) noexcept \
    {                                            \
        mValue[0] op scale;                      \
        mValue[1] op scale;                      \
        mValue[2] op scale;                      \
        return *this;                            \
    }
    ALURE_DECL_OP(*=)
    ALURE_DECL_OP(/=)
#undef ALURE_DECL_OP

    constexpr ALfloat getLengthSquared() const noexcept
    { return mValue[0]*mValue[0] + mValue[1]*mValue[1] + mValue[2]*mValue[2]; }
    ALfloat getLength() const noexcept
    { return std::sqrt(getLengthSquared()); }

    constexpr ALfloat getDistanceSquared(const Vector3 &pos) const noexcept
    { return (pos - *this).getLengthSquared(); }
    ALfloat getDistance(const Vector3 &pos) const noexcept
    { return (pos - *this).getLength(); }
};
static_assert(sizeof(Vector3) == sizeof(ALfloat[3]), "Bad Vector3 size");


enum class SampleType {
    UInt8,
    Int16,
    Float32,
    Mulaw
};
ALURE_API const char *GetSampleTypeName(SampleType type);

enum class ChannelConfig {
    /** 1-channel mono sound. */
    Mono,
    /** 2-channel stereo sound. */
    Stereo,
    /** 2-channel rear sound (back-left and back-right). */
    Rear,
    /** 4-channel surround sound. */
    Quad,
    /** 5.1 surround sound. */
    X51,
    /** 6.1 surround sound. */
    X61,
    /** 7.1 surround sound. */
    X71,
    /** 3-channel B-Format, using FuMa channel ordering and scaling. */
    BFormat2D,
    /** 4-channel B-Format, using FuMa channel ordering and scaling. */
    BFormat3D
};
ALURE_API const char *GetChannelConfigName(ChannelConfig cfg);

ALURE_API ALuint FramesToBytes(ALuint frames, ChannelConfig chans, SampleType type);
ALURE_API ALuint BytesToFrames(ALuint bytes, ChannelConfig chans, SampleType type);


/**
 * Creates a version number value using the specified major and minor values.
 */
constexpr inline ALCuint MakeVersion(ALCushort major, ALCushort minor)
{ return (major<<16) | minor; }

/**
 * Retrieves the major version of a version number value created by
 * MakeVersion.
 */
constexpr inline ALCuint MajorVersion(ALCuint version)
{ return version>>16; }
/**
 * Retrieves the minor version of a version number value created by
 * MakeVersion.
 */
constexpr inline ALCuint MinorVersion(ALCuint version)
{ return version&0xffff; }


enum class DeviceEnumeration {
    Basic = ALC_DEVICE_SPECIFIER,
    Complete = ALC_ALL_DEVICES_SPECIFIER,
    Capture = ALC_CAPTURE_DEVICE_SPECIFIER
};

enum class DefaultDeviceType {
    Basic = ALC_DEFAULT_DEVICE_SPECIFIER,
    Complete = ALC_DEFAULT_ALL_DEVICES_SPECIFIER,
    Capture = ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER
};

/**
 * A class managing Device objects and other related functionality. This class
 * is a singleton, only one instance will exist in a process.
 */
class ALURE_API DeviceManager {
public:
    /** Retrieves the DeviceManager instance. */
    static DeviceManager &get();

    /** Queries the existence of a non-device-specific ALC extension. */
    virtual bool queryExtension(const String &name) const = 0;

    /** Enumerates available device names of the given type. */
    virtual Vector<String> enumerate(DeviceEnumeration type) const = 0;
    /** Retrieves the default device of the given type. */
    virtual String defaultDeviceName(DefaultDeviceType type) const = 0;

    /**
     * Opens the playback device given by name, or the default if blank. Throws
     * an exception on error.
     */
    virtual Device openPlayback(const String &name=String()) = 0;

    /**
     * Opens the playback device given by name, or the default if blank.
     * Returns an empty Device on error.
     */
    virtual Device openPlayback(const String &name, const std::nothrow_t&) = 0;

    /** Opens the default playback device. Returns an empty Device on error. */
    Device openPlayback(const std::nothrow_t&);
};


enum class PlaybackDeviceName {
    Basic = ALC_DEVICE_SPECIFIER,
    Complete = ALC_ALL_DEVICES_SPECIFIER
};

#define MAKE_PIMPL(BaseT, ImplT)                                              \
private:                                                                      \
    ImplT *pImpl;                                                             \
                                                                              \
public:                                                                       \
    using handle_type = ImplT*;                                               \
                                                                              \
    BaseT() : pImpl(nullptr) { }                                              \
    BaseT(ImplT *impl) : pImpl(impl) { }                                      \
    BaseT(const BaseT&) = default;                                            \
    BaseT(BaseT&& rhs) : pImpl(rhs.pImpl) { rhs.pImpl = nullptr; }            \
                                                                              \
    BaseT& operator=(const BaseT&) = default;                                 \
    BaseT& operator=(BaseT&& rhs)                                             \
    {                                                                         \
        pImpl = rhs.pImpl; rhs.pImpl = nullptr;                               \
        return *this;                                                         \
    }                                                                         \
                                                                              \
    bool operator==(const BaseT &rhs) const { return pImpl == rhs.pImpl; }    \
    bool operator==(BaseT&& rhs) const { return pImpl == rhs.pImpl; }         \
                                                                              \
    operator bool() const { return !!pImpl; }                                 \
                                                                              \
    handle_type getHandle() const { return pImpl; }

class ALURE_API Device {
    MAKE_PIMPL(Device, ALDevice)

public:
    /** Retrieves the device name as given by type. */
    String getName(PlaybackDeviceName type=PlaybackDeviceName::Basic) const;
    /** Queries the existence of an ALC extension on this device. */
    bool queryExtension(const String &name) const;

    /**
     * Retrieves the ALC version supported by this device, as constructed by
     * MakeVersion.
     */
    ALCuint getALCVersion() const;

    /**
     * Retrieves the EFX version supported by this device, as constructed by
     * MakeVersion. If the ALC_EXT_EFX extension is unsupported, this will be
     * 0.
     */
    ALCuint getEFXVersion() const;

    /** Retrieves the device's playback frequency, in hz. */
    ALCuint getFrequency() const;

    /**
     * Retrieves the maximum number of auxiliary source sends. If ALC_EXT_EFX
     * is unsupported, this will be 0.
     */
    ALCuint getMaxAuxiliarySends() const;

    /**
     * Enumerates available HRTF names. The names are sorted as OpenAL gives
     * them, such that the index of a given name is the ID to use with
     * ALC_HRTF_ID_SOFT.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    Vector<String> enumerateHRTFNames() const;

    /**
     * Retrieves whether HRTF is enabled on the device or not.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    bool isHRTFEnabled() const;

    /**
     * Retrieves the name of the HRTF currently being used by this device.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    String getCurrentHRTF() const;

    /**
     * Resets the device, using the specified attributes.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    void reset(ArrayView<AttributePair> attributes);

    /**
     * Creates a new Context on this device, using the specified attributes.
     */
    Context createContext(ArrayView<AttributePair> attributes=ArrayView<AttributePair>());

    /**
     * Pauses device processing, stopping updates for its contexts. Multiple
     * calls are allowed but it is not reference counted, so the device will
     * resume after one resumeDSP call.
     *
     * Requires the ALC_SOFT_pause_device extension.
     */
    void pauseDSP();

    /**
     * Resumes device processing, restarting updates for its contexts. Multiple
     * calls are allowed and will no-op.
     */
    void resumeDSP();

    /**
     * Closes and frees the device. All previously-created contexts must first
     * be destroyed.
     */
    void close();
};


enum class DistanceModel {
    InverseClamped  = AL_INVERSE_DISTANCE_CLAMPED,
    LinearClamped   = AL_LINEAR_DISTANCE_CLAMPED,
    ExponentClamped = AL_EXPONENT_DISTANCE_CLAMPED,
    Inverse  = AL_INVERSE_DISTANCE,
    Linear   = AL_LINEAR_DISTANCE,
    Exponent = AL_EXPONENT_DISTANCE,
    None  = AL_NONE,
};

class ALURE_API Context {
    MAKE_PIMPL(Context, ALContext)

public:
    /** Makes the specified context current for OpenAL operations. */
    static void MakeCurrent(Context context);
    /** Retrieves the current context used for OpenAL operations. */
    static Context GetCurrent();

    /**
     * Makes the specified context current for OpenAL operations on the calling
     * thread only. Requires the ALC_EXT_thread_local_context extension on both
     * the context's device and the DeviceManager.
     */
    static void MakeThreadCurrent(Context context);
    /** Retrieves the thread-specific context used for OpenAL operations. */
    static Context GetThreadCurrent();

    /**
     * Destroys the context. The context must not be current when this is
     * called.
     */
    void destroy();

    /** Retrieves the Device this context was created from. */
    Device getDevice();

    void startBatch();
    void endBatch();

    /**
     * Retrieves a Listener instance for this context. Each context will only
     * have one listener.
     */
    Listener getListener();

    /**
     * Sets a MessageHandler instance which will be used to provide certain
     * messages back to the application. Only one handler may be set for a
     * context at a time. The previously set handler will be returned.
     */
    SharedPtr<MessageHandler> setMessageHandler(SharedPtr<MessageHandler> handler);

    /** Gets the currently-set message handler. */
    SharedPtr<MessageHandler> getMessageHandler() const;

    /**
     * Specifies the desired interval that the background thread will be woken
     * up to process tasks, e.g. keeping streaming sources filled. An interval
     * of 0 means the background thread will only be woken up manually with
     * calls to update. The default is 0.
     */
    void setAsyncWakeInterval(std::chrono::milliseconds msec);

    /**
     * Retrieves the current interval used for waking up the background thread.
     */
    std::chrono::milliseconds getAsyncWakeInterval() const;

    // Functions below require the context to be current

    /**
     * Creates a Decoder instance for the given audio file or resource name.
     */
    SharedPtr<Decoder> createDecoder(const String &name);

    /**
     * Queries if the channel configuration and sample type are supported by
     * the context.
     */
    bool isSupported(ChannelConfig channels, SampleType type) const;

    /**
     * Queries the list of resamplers supported by the context. If the
     * AL_SOFT_source_resampler extension is unsupported this will be an empty
     * vector, otherwise there will be at least one entry.
     */
    const Vector<String> &getAvailableResamplers();
    /**
     * Queries the context's default resampler index. Be aware, if the
     * AL_SOFT_source_resampler extension is unsupported the resampler list
     * will be empty and this will resturn 0. If you try to access the
     * resampler list with this index without the extension, undefined behavior
     * (accessing an out of bounds array index) will occur.
     */
    ALsizei getDefaultResamplerIndex() const;

    /**
     * Creates and caches a Buffer for the given audio file or resource name.
     * Multiple calls with the same name will return the same Buffer object.
     */
    Buffer getBuffer(const String &name);

    /**
     * Creates and caches a Buffer for the given audio file or resource name.
     * Multiple calls with the same name will return the same Buffer object.
     *
     * The returned Buffer object will be scheduled for loading asynchronously,
     * and must be checked with a call to Buffer::getLoadStatus prior to being
     * played.
     */
    Buffer getBufferAsync(const String &name);

    /**
     * Creates and caches a Buffer using the given name. The name may alias an
     * audio file, but it must not currently exist in the buffer cache. As with
     * other cached buffers, removeBuffer must be used to remove it from the
     * cache.
     */
    Buffer createBufferFrom(const String &name, SharedPtr<Decoder> decoder);

    /**
     * Creates and caches a Buffer using the given name. The name may alias an
     * audio file, but it must not currently exist in the buffer cache.
     *
     * The returned Buffer object will be scheduled for loading asynchronously,
     * and must be checked with a call to Buffer::getLoadStatus prior to being
     * played. The given decoder will be held on to and used asynchronously to
     * load the buffer. The decoder must not have its read or seek methods
     * called while the buffer load status is pending.
     */
    Buffer createBufferAsyncFrom(const String &name, SharedPtr<Decoder> decoder);

    /**
     * Deletes the cached Buffer object for the given audio file or
     * resource name. The buffer must not be in use by a Source.
     */
    void removeBuffer(const String &name);
    /**
     * Deletes the given cached buffer. The buffer must not be in use by a
     * Source.
     */
    void removeBuffer(Buffer buffer);

    /**
     * Creates a new Source. There is no practical limit to the number of
     * sources you may create.
     */
    Source createSource();

    AuxiliaryEffectSlot createAuxiliaryEffectSlot();

    Effect createEffect();

    SourceGroup createSourceGroup(String name);
    SourceGroup getSourceGroup(const String &name);

    void setDopplerFactor(ALfloat factor);

    void setSpeedOfSound(ALfloat speed);

    void setDistanceModel(DistanceModel model);

    /**
     * Updates the context and all sources belonging to this context (you do
     * not need to call the individual sources' update method if you call this
     * function).
     */
    void update();
};

class ALURE_API Listener {
    MAKE_PIMPL(Listener, ALListener)

public:
    void setGain(ALfloat gain);

    void set3DParameters(const Vector3 &position, const Vector3 &velocity, std::pair<Vector3,Vector3> orientation);

    void setPosition(ALfloat x, ALfloat y, ALfloat z);
    void setPosition(const ALfloat *pos);

    void setVelocity(ALfloat x, ALfloat y, ALfloat z);
    void setVelocity(const ALfloat *vel);

    void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2);
    void setOrientation(const ALfloat *at, const ALfloat *up);
    void setOrientation(const ALfloat *ori);

    void setMetersPerUnit(ALfloat m_u);
};


enum class BufferLoadStatus {
    Pending,
    Ready
};

class ALURE_API Buffer {
    MAKE_PIMPL(Buffer, ALBuffer)

public:
    /**
     * Retrieves the length of the buffer in sample frames. The buffer must be
     * fully loaded before this method is called.
     */
    ALuint getLength() const;

    /** Retrieves the buffer's frequency in hz. */
    ALuint getFrequency() const;

    /** Retrieves the buffer's sample configuration. */
    ChannelConfig getChannelConfig() const;

    /** Retrieves the buffer's sample type. */
    SampleType getSampleType() const;

    /**
     * Retrieves the storage size used by the buffer, in bytes. The buffer must
     * be fully loaded before this method is called.
     */
    ALuint getSize() const;

    /**
     * Sets the buffer's loop points, used for looping sources. If the current
     * context does not support the AL_SOFT_loop_points extension, start and
     * end must be 0 and getLength() respectively. Otherwise, start must be
     * less than end, and end must be less than or equal to getLength().
     *
     * The buffer must not be in use when this method is called, and the buffer
     * must be fully loaded.
     *
     * \param start The starting point, in sample frames (inclusive).
     * \param end The ending point, in sample frames (exclusive).
     */
    void setLoopPoints(ALuint start, ALuint end);

    /**
     * Retrieves the current loop points as a [start,end) pair. The buffer must
     * be fully loaded before this method is called.
     */
    std::pair<ALuint,ALuint> getLoopPoints() const;

    /**
     * Retrieves the Source objects currently playing the buffer. Stopping the
     * returned sources will allow the buffer to be removed from the context.
     */
    Vector<Source> getSources() const;

    /**
     * Queries the buffer's load status. A return of BufferLoadStatus::Pending
     * indicates the buffer is not finished loading and can't be used with a
     * call to Source::play. Buffers created with Context::getBuffer will
     * always return BufferLoadStatus::Ready.
     */
    BufferLoadStatus getLoadStatus();

    /** Retrieves the name the buffer was created with. */
    const String &getName() const;

    /** Queries if the buffer is in use and can't be removed. */
    bool isInUse() const;
};


enum class Spatialize {
    Off = AL_FALSE,
    On = AL_TRUE,
    Auto = 0x0002 /* AL_AUTO_SOFT */
};

class ALURE_API Source {
    MAKE_PIMPL(Source, ALSource)

public:
    /**
     * Plays the source using buffer. The same buffer may be played from
     * multiple sources simultaneously.
     */
    void play(Buffer buffer);
    /**
     * Plays the source by streaming audio from decoder. This will use
     * queuesize buffers, each with updatelen sample frames. The given decoder
     * must *NOT* have its read or seek methods called from elsewhere while in
     * use.
     */
    void play(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint queuesize);
    /**
     * Stops playback, releasing the buffer or decoder reference.
     */
    void stop();

    /** Pauses the source if it is playing. */
    void pause();

    /** Resumes the source if it is paused. */
    void resume();

    /** Specifies if the source is currently playing. */
    bool isPlaying() const;

    /** Specifies if the source is currently paused. */
    bool isPaused() const;

    /**
     * Specifies the source's playback priority. Lowest priority sources will
     * be evicted first when higher priority sources are played.
     */
    void setPriority(ALuint priority);
    /** Retrieves the source's priority. */
    ALuint getPriority() const;

    /**
     * Sets the source's offset, in sample frames. If the source is playing or
     * paused, it will go to that offset immediately, otherwise the source will
     * start at the specified offset the next time it's played.
     */
    void setOffset(uint64_t offset);
    /**
     * Retrieves the source offset in sample frames and its latency in nano-
     * seconds. For streaming sources, this will be the offset from the
     * beginning of the stream based on the decoder's reported position.
     *
     * If the AL_SOFT_source_latency extension is unsupported, the latency will
     * be 0.
     */
    std::pair<uint64_t,std::chrono::nanoseconds> getSampleOffsetLatency() const;
    uint64_t getSampleOffset() const { return std::get<0>(getSampleOffsetLatency()); }
    /**
     * Retrieves the source offset and latency in seconds. For streaming
     * sources, this will be the offset from the beginning of the stream based
     * on the decoder's reported position.
     *
     * If the AL_SOFT_source_latency extension is unsupported, the latency will
     * be 0.
     */
    std::pair<Seconds,Seconds> getSecOffsetLatency() const;
    Seconds getSecOffset() const { return std::get<0>(getSecOffsetLatency()); }

    /**
     * Specifies if the source should loop on the Buffer or Decoder object's
     * loop points.
     */
    void setLooping(bool looping);
    bool getLooping() const;

    /**
     * Specifies a linear pitch shift base. A value of 1.0 is the default
     * normal speed.
     */
    void setPitch(ALfloat pitch);
    ALfloat getPitch() const;

    /**
     * Specifies the base linear gain. A value of 1.0 is the default normal
     * volume.
     */
    void setGain(ALfloat gain);
    ALfloat getGain() const;

    /**
     * Specifies the minimum and maximum gain. The source's gain is clamped to
     * this range after distance attenuation and cone attenuation are applied
     * to the gain base, although before the filter gain adjustements.
     */
    void setGainRange(ALfloat mingain, ALfloat maxgain);
    std::pair<ALfloat,ALfloat> getGainRange() const;
    ALfloat getMinGain() const { return std::get<0>(getGainRange()); }
    ALfloat getMaxGain() const { return std::get<1>(getGainRange()); }

    /**
     * Specifies the reference distance and maximum distance the source will
     * use for the current distance model. For Clamped distance models, the
     * source's calculated distance is clamped to the specified range before
     * applying distance-related attenuation.
     *
     * For all distance models, the reference distance is the distance at which
     * the source's volume will not have any extra attenuation (an effective
     * gain multiplier of 1).
     */
    void setDistanceRange(ALfloat refdist, ALfloat maxdist);
    std::pair<ALfloat,ALfloat> getDistanceRange() const;
    ALfloat getReferenceDistance() const { return std::get<0>(getDistanceRange()); }
    ALfloat getMaxDistance() const { return std::get<1>(getDistanceRange()); }

    /** Specifies the source's 3D position, velocity, and direction together. */
    void set3DParameters(const Vector3 &position, const Vector3 &velocity, const Vector3 &direction);

    /** Specifies the source's 3D position, velocity, and orientation together. */
    void set3DParameters(const Vector3 &position, const Vector3 &velocity, std::pair<Vector3,Vector3> orientation);

    /** Specifies the source's 3D position. */
    void setPosition(ALfloat x, ALfloat y, ALfloat z);
    void setPosition(const ALfloat *pos);
    Vector3 getPosition() const;

    /**
     * Specifies the source's 3D velocity, in units per second. As with OpenAL,
     * this does not actually alter the source's position, and instead just
     * alters the pitch as determined by the doppler effect.
     */
    void setVelocity(ALfloat x, ALfloat y, ALfloat z);
    void setVelocity(const ALfloat *vel);
    Vector3 getVelocity() const;

    /**
     * Specifies the source's 3D facing direction. Deprecated in favor of
     * setOrientation.
     */
    void setDirection(ALfloat x, ALfloat y, ALfloat z);
    void setDirection(const ALfloat *dir);
    Vector3 getDirection() const;

    /**
     * Specifies the source's 3D orientation. Note: unlike the AL_EXT_BFORMAT
     * extension this property comes from, this also affects the facing
     * direction, superceding setDirection.
     */
    void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2);
    void setOrientation(const ALfloat *at, const ALfloat *up);
    void setOrientation(const ALfloat *ori);
    std::pair<Vector3,Vector3> getOrientation() const;

    /**
     * Specifies the source's cone angles, in degrees. The inner angle is the
     * area within which the listener will hear the source with no extra
     * attenuation, while the listener being outside of the outer angle will
     * hear the source attenuated according to the outer cone gains.
     */
    void setConeAngles(ALfloat inner, ALfloat outer);
    std::pair<ALfloat,ALfloat> getConeAngles() const;
    ALfloat getInnerConeAngle() const { return std::get<0>(getConeAngles()); }
    ALfloat getOuterConeAngle() const { return std::get<1>(getConeAngles()); }

    /**
     * Specifies the linear gain multiplier when the listener is outside of the
     * source's outer cone area. The specified gain applies to all frequencies,
     * while gainhf applies extra attenuation to high frequencies.
     *
     * \param gainhf has no effect without the ALC_EXT_EFX extension.
     */
    void setOuterConeGains(ALfloat gain, ALfloat gainhf=1.0f);
    std::pair<ALfloat,ALfloat> getOuterConeGains() const;
    ALfloat getOuterConeGain() const { return std::get<0>(getOuterConeGains()); }
    ALfloat getOuterConeGainHF() const { return std::get<1>(getOuterConeGains()); }

    /**
     * Specifies the rolloff factors for the direct and send paths. This is
     * effectively a distance scaling relative to the reference distance. Note:
     * the room rolloff factor is 0 by default, disabling distance attenuation
     * for send paths. This is because the reverb engine will, by default,
     * apply a more realistic room attenuation based on the reverb decay time
     * and direct path attenuation.
     */
    void setRolloffFactors(ALfloat factor, ALfloat roomfactor=0.0f);
    std::pair<ALfloat,ALfloat> getRolloffFactors() const;
    ALfloat getRolloffFactor() const { return std::get<0>(getRolloffFactors()); }
    ALfloat getRoomRolloffFactor() const { return std::get<1>(getRolloffFactors()); }

    /**
     * Specifies the doppler factor for the doppler effect's pitch shift. This
     * effectively scales the source and listener velocities for the doppler
     * calculation.
     */
    void setDopplerFactor(ALfloat factor);
    ALfloat getDopplerFactor() const;

    /** Specifies if the source properties are relative to the listener. */
    void setRelative(bool relative);
    bool getRelative() const;

    /**
     * Specifies the source's radius. This causes the source to behave as if
     * every point within the spherical area emits sound.
     *
     * Has no effect without the AL_EXT_SOURCE_RADIUS extension.
     */
    void setRadius(ALfloat radius);
    ALfloat getRadius() const;

    /**
     * Specifies the left and right channel angles, in radians, when playing a
     * stereo buffer or stream. The angles go counter-clockwise, with 0 being
     * in front and positive values going left.
     *
     * Has no effect without the AL_EXT_STEREO_ANGLES extension.
     */
    void setStereoAngles(ALfloat leftAngle, ALfloat rightAngle);
    std::pair<ALfloat,ALfloat> getStereoAngles() const;

    void set3DSpatialize(Spatialize spatialize);
    Spatialize get3DSpatialize() const;

    void setResamplerIndex(ALsizei index);
    ALsizei getResamplerIndex() const;

    void setAirAbsorptionFactor(ALfloat factor);
    ALfloat getAirAbsorptionFactor() const;

    void setGainAuto(bool directhf, bool send, bool sendhf);
    std::tuple<bool,bool,bool> getGainAuto() const;
    bool getDirectGainHFAuto() const { return std::get<0>(getGainAuto()); }
    bool getSendGainAuto() const { return std::get<1>(getGainAuto()); }
    bool getSendGainHFAuto() const { return std::get<2>(getGainAuto()); }

    /** Sets the filter properties on the direct path signal. */
    void setDirectFilter(const FilterParams &filter);
    /**
     * Sets the filter properties on the given send path signal. Any auxiliary
     * effect slot on the send path remains in place.
     */
    void setSendFilter(ALuint send, const FilterParams &filter);
    /**
     * Connects the effect slot slot to the given send path. Any filter
     * properties on the send path remain as they were.
     */
    void setAuxiliarySend(AuxiliaryEffectSlot slot, ALuint send);
    /**
     * Connects the effect slot slot to the given send path, using the filter
     * properties.
     */
    void setAuxiliarySendFilter(AuxiliaryEffectSlot slot, ALuint send, const FilterParams &filter);

    /**
     * Updates the source, ensuring that resources are released when playback
     * is finished.
     */
    void update();

    /**
     * Releases the source, stopping playback, releasing resources, and
     * returning it to the system.
     */
    void release();
};


class ALURE_API SourceGroup {
    MAKE_PIMPL(SourceGroup, ALSourceGroup)

public:
    /** Retrieves the associated name of the source group. */
    const String &getName() const;

    /**
     * Adds source to the source group. A source may only be part of one group
     * at a time, and will automatically be removed from its current group as
     * needed.
     */
    void addSource(Source source);
    /** Removes source from the source group. */
    void removeSource(Source source);

    /** Adds a list of sources to the group at once. */
    void addSources(ArrayView<Source> sources);
    /** Removes a list of sources from the source group. */
    void removeSources(ArrayView<Source> sources);

    /**
     * Adds group as a subgroup of the source group. This method will throw an
     * exception if group is being added to a group it has as a sub-group (i.e.
     * it would create a circular sub-group chain).
     */
    void addSubGroup(SourceGroup group);
    /** Removes group from the source group. */
    void removeSubGroup(SourceGroup group);

    /** Returns the list of sources currently in the group. */
    Vector<Source> getSources() const;

    /** Returns the list of subgroups currently in the group. */
    Vector<SourceGroup> getSubGroups() const;

    /** Sets the source group gain, which accumulates with its sources. */
    void setGain(ALfloat gain);
    /** Gets the source group gain. */
    ALfloat getGain() const;

    /** Sets the source group pitch, which accumulates with its sources. */
    void setPitch(ALfloat pitch);
    /** Gets the source group pitch. */
    ALfloat getPitch() const;

    /**
     * Pauses all currently-playing sources that are under this group,
     * including sub-groups.
     */
    void pauseAll() const;
    /**
     * Resumes all paused sources that are under this group, including
     * sub-groups.
     */
    void resumeAll() const;

    /** Stops all sources that are under this group, including sub-groups. */
    void stopAll() const;

    /**
     * Releases the source group, removing all sources from it before being
     * freed.
     */
    void release();
};


struct SourceSend {
    Source mSource;
    ALuint mSend;
};

class ALURE_API AuxiliaryEffectSlot {
    MAKE_PIMPL(AuxiliaryEffectSlot, ALAuxiliaryEffectSlot)

public:
    void setGain(ALfloat gain);
    /**
     * If set to true, the reverb effect will automatically apply adjustments
     * to the source's send slot based on the effect properties.
     *
     * Has no effect when using non-reverb effects. Default is true.
     */
    void setSendAuto(bool sendauto);

    /**
     * Updates the effect slot with a new effect. The given effect object may
     * be altered or destroyed without affecting the effect slot.
     */
    void applyEffect(Effect effect);

    /**
     * Releases the effect slot, returning it to the system. It must not be in
     * use by a source.
     */
    void release();

    /**
     * Retrieves each Source object and its pairing send this effect slot is
     * set on. Setting a different (or null) effect slot on each source's given
     * send will allow the effect slot to be released.
     */
    Vector<SourceSend> getSourceSends() const;

    /** Determines if the effect slot is in use by a source. */
    bool isInUse() const;
};


class ALURE_API Effect {
    MAKE_PIMPL(Effect, ALEffect)

public:
    /**
     * Updates the effect with the specified reverb properties. If the
     * EAXReverb effect is not supported, it will automatically attempt to
     * downgrade to the Standard Reverb effect.
     */
    void setReverbProperties(const EFXEAXREVERBPROPERTIES &props);

    void destroy();
};


/**
 * Audio decoder interface. Applications may derive from this, implementing the
 * necessary methods, and use it in places the API wants a Decoder object.
 */
class ALURE_API Decoder {
public:
    virtual ~Decoder() { }

    /** Retrieves the sample frequency, in hz, of the audio being decoded. */
    virtual ALuint getFrequency() const = 0;
    /** Retrieves the channel configuration of the audio being decoded. */
    virtual ChannelConfig getChannelConfig() const = 0;
    /** Retrieves the sample type of the audio being decoded. */
    virtual SampleType getSampleType() const = 0;

    /**
     * Retrieves the total length of the audio, in sample frames. If unknown,
     * returns 0. Note that if the returned length is 0, the decoder may not be
     * used to load a Buffer.
     */
    virtual uint64_t getLength() const = 0;
    /**
     * Retrieves the current sample frame position (i.e. the number of sample
     * frames from the beginning).
     */
    virtual uint64_t getPosition() const = 0;
    /**
     * Seek to pos, specified in sample frames. Returns true if the seek was
     * successful.
     */
    virtual bool seek(uint64_t pos) = 0;

    /**
     * Retrieves the loop points, in sample frames, as a [start,end) pair. If
     * start >= end, use all available data.
     */
    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const = 0;

    /**
     * Decodes count sample frames, writing them to ptr, and returns the number
     * of sample frames written. Returning less than the requested count
     * indicates the end of the audio.
     */
    virtual ALuint read(ALvoid *ptr, ALuint count) = 0;
};

/**
 * Audio decoder factory interface. Applications may derive from this,
 * implementing the necessary methods, and use it in places the API wants a
 * DecoderFactory object.
 */
class ALURE_API DecoderFactory {
public:
    virtual ~DecoderFactory() { }

    /**
     * Creates and returns a Decoder instance for the given resource file. If
     * the decoder needs to retain the file handle for reading as-needed, it
     * should move the UniquePtr to internal storage.
     *
     * \return nullptr if a decoder can't be created from the file.
     */
    virtual SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) = 0;
};

/**
 * Registers a decoder factory for decoding audio. Registered factories are
 * used in lexicographical order, e.g. if Factory1 is registered with name1 and
 * Factory2 is registered with name2, Factory1 will be used before Factory2 if
 * name1 < name2. Internal decoder factories are always used after registered
 * ones.
 *
 * Alure retains a reference to the DecoderFactory instance and will release it
 * (potentially destroying the object) when the library unloads.
 *
 * \param name A unique name identifying this decoder factory.
 * \param factory A DecoderFactory instance used to create Decoder instances.
 */
ALURE_API void RegisterDecoder(const String &name, UniquePtr<DecoderFactory> factory);

/**
 * Unregisters a decoder factory by name. Alure returns the instance back to
 * the application.
 *
 * \param name The unique name identifying a previously-registered decoder
 * factory.
 *
 * \return The unregistered decoder factory instance, or 0 (nullptr) if a
 * decoder factory with the given name doesn't exist.
 */
ALURE_API UniquePtr<DecoderFactory> UnregisterDecoder(const String &name);


/**
 * A file I/O factory interface. Applications may derive from this and set an
 * instance to be used by the audio decoders. By default, the library uses
 * standard I/O.
 */
class ALURE_API FileIOFactory {
public:
    /**
     * Sets the factory instance to be used by the audio decoders. If a
     * previous factory was set, it's returned to the application. Passing in a
     * NULL factory reverts to the default.
     */
    static UniquePtr<FileIOFactory> set(UniquePtr<FileIOFactory> factory);

    /**
     * Gets the current FileIOFactory instance being used by the audio
     * decoders.
     */
    static FileIOFactory &get();

    virtual ~FileIOFactory() { }

    /** Opens a read-only binary file for the given name. */
    virtual UniquePtr<std::istream> openFile(const String &name) = 0;
};


/**
 * A message handler interface. Applications may derive from this and set an
 * instance on a context to receive messages. The base methods are no-ops, so
 * derived classes only need to implement methods for relevant messages.
 *
 * It's recommended that applications mark their handler methods using the
 * override keyword, to ensure they're properly overriding the base methods in
 * case they change.
 */
class ALURE_API MessageHandler {
public:
    virtual ~MessageHandler();

    /**
     * Called when the given device has been disconnected and is no longer
     * usable for output. As per the ALC_EXT_disconnect specification,
     * disconnected devices remain valid, however all playing sources are
     * automatically stopped, any sources that are attempted to play will
     * immediately stop, and new contexts may not be created on the device.
     *
     * Note that connection status is checked during Context::update calls, so
     * that method must be called regularly to be notified when a device is
     * disconnected. This method may not be called if the device lacks support
     * for the ALC_EXT_disconnect extension.
     *
     * WARNING: Do not attempt to clean up resources within this callback
     * method, as Alure is in the middle of doing updates. Instead, flag the
     * device as having been lost and do cleanup later.
     */
    virtual void deviceDisconnected(Device device);

    /**
     * Called when the given source reaches the end of the buffer or stream.
     *
     * Sources that stopped automatically will be detected upon a call to
     * Context::update or Source::update.
     */
    virtual void sourceStopped(Source source);

    /**
     * Called when the given source was forced to stop. This can be because
     * either there were no more system sources and a higher-priority source
     * needs to play, or it's part of a SourceGroup (or sub-group thereof) that
     * had its SourceGroup::stopAll method called.
     */
    virtual void sourceForceStopped(Source source);

    /**
     * Called when a new buffer is about to be created and loaded. May be
     * called asynchronously for buffers being loaded asynchronously.
     *
     * \param name The resource name, as passed to Context::getBuffer.
     * \param channels Channel configuration of the given audio data.
     * \param type Sample type of the given audio data.
     * \param samplerate Sample rate of the given audio data.
     * \param data The audio data that is about to be fed to the OpenAL buffer.
     */
    virtual void bufferLoading(const String &name, ChannelConfig channels, SampleType type, ALuint samplerate, const Vector<ALbyte> &data);

    /**
     * Called when a resource isn't found, allowing the app to substitute in a
     * different resource. For buffers created with Context::getBuffer or
     * Context::getBufferAsync, the original name will still be used for the
     * cache map so the app doesn't have to keep track of substituted resource
     * names.
     *
     * This will be called again if the new name isn't found.
     *
     * \param name The resource name that was not found.
     * \return The replacement resource name to use instead. Returning an empty
     *         string means to stop trying.
     */
    virtual String resourceNotFound(const String &name);
};

} // namespace alure

#endif /* AL_ALURE2_H */
