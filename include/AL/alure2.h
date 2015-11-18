#ifndef AL_ALURE2_H
#define AL_ALURE2_H

#include <vector>
#include <string>
#include <memory>
#include <cmath>

#include "alc.h"
#include "al.h"

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
class Context;
class Listener;
class Buffer;
class Source;
class AuxiliaryEffectSlot;
class Effect;
class Decoder;
class DecoderFactory;
class MessageHandler;


// A SharedPtr implementation, defaults to C++11's std::shared_ptr. If this is
// changed, you must recompile the library.
template<typename T>
using SharedPtr = std::shared_ptr<T>;


struct FilterParams {
    ALfloat mGain;
    ALfloat mGainHF; // For low-pass and band-pass filters
    ALfloat mGainLF; // For high-pass and band-pass filters
};


class Vector3 {
    ALfloat mValue[3];

public:
    Vector3() { }
    Vector3(const Vector3 &rhs) : mValue{rhs.mValue[0], rhs.mValue[1], rhs.mValue[2]}
    { }
    Vector3(ALfloat val) : mValue{val, val, val}
    { }
    Vector3(ALfloat x, ALfloat y, ALfloat z) : mValue{x, y, z}
    { }
    Vector3(const ALfloat *vec) : mValue{vec[0], vec[1], vec[2]}
    { }

    const ALfloat *getPtr() const
    { return mValue; }

    ALfloat& operator[](size_t i)
    { return mValue[i]; }
    const ALfloat& operator[](size_t i) const
    { return mValue[i]; }

#define ALURE_DECL_OP(op)                           \
    Vector3 operator op(const Vector3 &rhs) const   \
    {                                               \
        return Vector3(mValue[0] op rhs.mValue[0],  \
                       mValue[1] op rhs.mValue[1],  \
                       mValue[2] op rhs.mValue[2]); \
    }
    ALURE_DECL_OP(+)
    ALURE_DECL_OP(-)
    ALURE_DECL_OP(*)
    ALURE_DECL_OP(/)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                    \
    Vector3& operator op(const Vector3 &rhs) \
    {                                        \
        mValue[0] op rhs.mValue[0];          \
        mValue[1] op rhs.mValue[1];          \
        mValue[2] op rhs.mValue[2];          \
        return *this;                        \
    }
    ALURE_DECL_OP(+=)
    ALURE_DECL_OP(-=)
    ALURE_DECL_OP(*=)
    ALURE_DECL_OP(/=)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)                    \
    Vector3 operator op(ALfloat scale) const \
    {                                        \
        return Vector3(mValue[0] op scale,   \
                       mValue[1] op scale,   \
                       mValue[2] op scale);  \
    }
    ALURE_DECL_OP(*)
    ALURE_DECL_OP(/)
#undef ALURE_DECL_OP
#define ALURE_DECL_OP(op)               \
    Vector3& operator op(ALfloat scale) \
    {                                   \
        mValue[0] op scale;             \
        mValue[1] op scale;             \
        mValue[2] op scale;             \
        return *this;                   \
    }
    ALURE_DECL_OP(*=)
    ALURE_DECL_OP(/=)
#undef ALURE_DECL_OP

    ALfloat getLengthSquared() const
    { return mValue[0]*mValue[0] + mValue[1]*mValue[1] + mValue[2]*mValue[2]; }
    ALfloat getLength() const
    { return sqrtf(getLengthSquared()); }

    ALfloat getDistanceSquared(const Vector3 &pos) const
    { return (pos - *this).getLengthSquared(); }
    ALfloat getDistance(const Vector3 &pos) const
    { return (pos - *this).getLength(); }
};
static_assert(sizeof(Vector3) == sizeof(ALfloat[3]), "Bad Vector3 size");


/**
 * Creates a version number value using the specified \param major and
 * \param minor values.
 */
inline ALCuint MakeVersion(ALCushort major, ALCushort minor)
{ return (major<<16) | minor; }

/**
 * Retrieves the major version of a version number value created by
 * \ref MakeVersion.
 */
inline ALCuint MajorVersion(ALCuint version)
{ return version>>16; }
/**
 * Retrieves the minor version of a version number value created by
 * \ref MakeVersion.
 */
inline ALCuint MinorVersion(ALCuint version)
{ return version&0xffff; }


enum DeviceEnumeration {
    DevEnum_Basic = ALC_DEVICE_SPECIFIER,
    DevEnum_Complete = ALC_ALL_DEVICES_SPECIFIER,
    DevEnum_Capture = ALC_CAPTURE_DEVICE_SPECIFIER
};

enum DefaultDeviceType {
    DefaultDevType_Basic = ALC_DEFAULT_DEVICE_SPECIFIER,
    DefaultDevType_Complete = ALC_DEFAULT_ALL_DEVICES_SPECIFIER,
    DefaultDevType_Capture = ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER
};

/**
 * A class managing \ref Device objects and other related functionality. This
 * class is a singleton, only one instance will exist in a process.
 */
class DeviceManager {
public:
    /** Retrieves the DeviceManager instance. */
    static DeviceManager *get();

    /** Queries the existence of a non-device-specific ALC extension. */
    virtual bool queryExtension(const char *extname) const = 0;

    /** Enumerates available device names of the given \param type. */
    virtual std::vector<std::string> enumerate(DeviceEnumeration type) const = 0;
    /** Retrieves the default device of the given \param type. */
    virtual std::string defaultDeviceName(DefaultDeviceType type) const = 0;

    /** Opens the playback device given by \param name, or the default if empty. */
    virtual Device *openPlayback(const std::string &name=std::string()) = 0;
};


enum PlaybackDeviceType {
    PlaybackDevType_Basic = ALC_DEVICE_SPECIFIER,
    PlaybackDevType_Complete = ALC_ALL_DEVICES_SPECIFIER
};

class Device {
public:
    /** Retrieves the device name as given by \param type. */
    virtual std::string getName(PlaybackDeviceType type) const = 0;
    /** Queries the existence of an ALC extension on this device. */
    virtual bool queryExtension(const char *extname) const = 0;

    /**
     * Retrieves the ALC version supported by this device, as constructed by
     * \ref MakeVersion.
     */
    virtual ALCuint getALCVersion() const = 0;

    /**
     * Retrieves the EFX version supported by this device, as constructed by
     * \ref MakeVersion. If the ALC_EXT_EFX extension is unsupported, this
     * will be 0.
     */
    virtual ALCuint getEFXVersion() const = 0;

    /** Retrieves the device's playback frequency, in hz. */
    virtual ALCuint getFrequency() const = 0;

    /**
     * Retrieves the maximum number of auxiliary source sends. If ALC_EXT_EFX
     * is unsupported, this will be 0.
     */
    virtual ALCuint getMaxAuxiliarySends() const = 0;

    /**
     * Enumerates available HRTF names. The names are sorted as OpenAL gives
     * them, such that the index of a given name is the ID to use with
     * ALC_HRTF_ID_SOFT.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    virtual std::vector<std::string> enumerateHRTFNames() const = 0;

    /**
     * Retrieves whether HRTF is enabled on the device or not.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    virtual bool isHRTFEnabled() const = 0;

    /**
     * Retrieves the name of the HRTF currently being used by this device.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    virtual std::string getCurrentHRTF() const = 0;

    /**
     * Resets the device, using the specified \param attributes.
     *
     * Requires the ALC_SOFT_HRTF extension.
     */
    virtual void reset(ALCint *attributes) = 0;

    /**
     * Creates a new \ref Context on this device, using the specified
     * \param attributes.
     */
    virtual Context *createContext(ALCint *attributes=0) = 0;

    /**
     * Pauses device processing, stopping updates for its contexts. Multiple
     * calls are allowed but it is not reference counted, so the device will
     * resume after one \ref resumeDSP call.
     *
     * Requires the ALC_SOFT_pause_device extension.
     */
    virtual void pauseDSP() = 0;

    /**
     * Resumes device processing, restarting updates for its contexts. Multiple
     * calls are allowed and will no-op.
     */
    virtual void resumeDSP() = 0;

    /**
     * Closes and frees the device. All previously-created contexts must first
     * be destroyed.
     */
    virtual void close() = 0;
};


enum DistanceModel {
    DistanceModel_InverseClamped  = AL_INVERSE_DISTANCE_CLAMPED,
    DistanceModel_LinearClamped   = AL_LINEAR_DISTANCE_CLAMPED,
    DistanceModel_ExponentClamped = AL_EXPONENT_DISTANCE_CLAMPED,
    DistanceModel_Inverse  = AL_INVERSE_DISTANCE,
    DistanceModel_Linear   = AL_LINEAR_DISTANCE,
    DistanceModel_Exponent = AL_EXPONENT_DISTANCE,
    DistanceModel_None  = AL_NONE,
};

class Context {
public:
    /** Makes the specified \param context current for OpenAL operations. */
    static void MakeCurrent(Context *context);
    /** Retrieves the current context used for OpenAL operations. */
    static Context *GetCurrent();

    /**
     * Makes the specified \param context current for OpenAL operations on the
     * calling thread only. Requires the ALC_EXT_thread_local_context extension.
     */
    static void MakeThreadCurrent(Context *context);
    /**
     * Retrieves the thread-specific context used for OpenAL operations.
     * Requires the ALC_EXT_thread_local_context extension.
     */
    static Context *GetThreadCurrent();

    /**
     * Destroys the context. The context must not be current when this is
     * called.
     */
    virtual void destroy() = 0;

    /** Retrieves the \ref Device this context was created from. */
    virtual Device *getDevice() = 0;

    virtual void startBatch() = 0;
    virtual void endBatch() = 0;

    /**
     * Retrieves a \ref Listener instance for this context. Each context will
     * only have one listener.
     */
    virtual Listener *getListener() = 0;

    /**
     * Sets a MessageHandler instance which will be used to provide certain
     * messages back to the application. Only one handler may be set for a
     * context at a time. The previously set handler will be returned.
     */
    virtual SharedPtr<MessageHandler> setMessageHandler(SharedPtr<MessageHandler> handler) = 0;

    /** Gets the currently-set message handler. */
    virtual SharedPtr<MessageHandler> getMessageHandler() const = 0;

    /**
     * Creates a \ref Decoder instance for the given audio file or resource
     * \param name.
     */
    virtual SharedPtr<Decoder> createDecoder(const std::string &name) = 0;

    // Functions below require the context to be current

    /**
     * Creates and caches a \ref Buffer for the given audio file or resource
     * \param name. Multiple calls with the same name will return the same
     * \ref Buffer object.
     */
    virtual Buffer *getBuffer(const std::string &name) = 0;

    /**
     * Creates and caches a \ref Buffer for the given audio file or resource
     * \param name. Multiple calls with the same name will return the same
     * \ref Buffer object.
     *
     * The returned \ref Buffer object will be scheduled for loading
     * asynchronously, and must be checked with a call to
     * \ref Buffer::getLoadStatus prior to being played.
     */
    virtual Buffer *getBufferAsync(const std::string &name) = 0;

    /**
     * Deletes the cached \ref Buffer object for the given audio file or
     * resource \param name. The buffer must not be in use by a \ref Source.
     */
    virtual void removeBuffer(const std::string &name) = 0;
    /**
     * Deletes the given cached \param buffer instance. The buffer must not be
     * in use by a \ref Source.
     */
    virtual void removeBuffer(Buffer *buffer) = 0;

    /**
     * Gets a new \ref Source. There is no practical limit to the number of
     * sources you may get.
     */
    virtual Source *getSource() = 0;

    virtual AuxiliaryEffectSlot *createAuxiliaryEffectSlot() = 0;

    virtual Effect *createEffect() = 0;

    virtual void setDopplerFactor(ALfloat factor) = 0;

    virtual void setSpeedOfSound(ALfloat speed) = 0;

    virtual void setDistanceModel(DistanceModel model) = 0;

    /**
     * Updates the context and all sources belonging to this context (you do
     * not need to call the individual sources' update method if you call this
     * function).
     */
    virtual void update() = 0;
};


enum SampleType {
    SampleType_UInt8,
    SampleType_Int16,
    SampleType_Float32,
    SampleType_Mulaw
};
const char *GetSampleTypeName(SampleType type);

enum ChannelConfig {
    ChannelConfig_Mono,
    ChannelConfig_Stereo,
    ChannelConfig_Rear,
    ChannelConfig_Quad,
    ChannelConfig_X51,
    ChannelConfig_X61,
    ChannelConfig_X71,
    ChannelConfig_BFmt_WXY,
    ChannelConfig_BFmt_WXYZ
};
const char *GetChannelConfigName(ChannelConfig cfg);

enum BufferLoadStatus {
    BufferLoad_Pending,
    BufferLoad_Ready
};

class Listener {
public:
    virtual void setGain(ALfloat gain) = 0;

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setPosition(const ALfloat *pos) = 0;

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setVelocity(const ALfloat *vel) = 0;

    virtual void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) = 0;
    virtual void setOrientation(const ALfloat *at, const ALfloat *up) = 0;
    virtual void setOrientation(const ALfloat *ori) = 0;
};


class Buffer {
public:
    /**
     * Retrieves the length of the buffer in sample frames. The buffer must be
     * fully loaded before this method is called.
     */
    virtual ALuint getLength() const = 0;

    /** Retrieves the buffer's frequency in hz. */
    virtual ALuint getFrequency() const = 0;

    /** Retrieves the buffer's sample configuration. */
    virtual ChannelConfig getChannelConfig() const = 0;

    /** Retrieves the buffer's sample type. */
    virtual SampleType getSampleType() const = 0;

    /**
     * Retrieves the storage size used by the buffer, in bytes. The buffer must
     * be fully loaded before this method is called.
     */
    virtual ALuint getSize() const = 0;

    /**
     * Sets the buffer's loop points, used for looping sources. If the current
     * context does not support the AL_SOFT_loop_points extension, \param start
     * and \param end must be 0 and \ref getLength() respectively. Otherwise,
     * \param start must be less than \param end, and \param end must be less
     * than or equal to \ref getLength().
     *
     * The buffer must not be in use when this method is called, and the buffer
     * must be fully loaded.
     *
     * \param start The starting point, in sample frames (inclusive).
     * \param end The ending point, in sample frames (exclusive).
     */
    virtual void setLoopPoints(ALuint start, ALuint end) = 0;

    /**
     * Retrieves the current loop points as a [start,end) pair. The buffer must
     * be fully loaded before this method is called.
     */
    virtual std::pair<ALuint,ALuint> getLoopPoints() const = 0;

    /**
     * Retrieves the \ref Source objects currently playing the buffer. Stopping
     * the returned sources will allow the buffer to be removed from the
     * context.
     */
    virtual std::vector<Source*> getSources() const = 0;

    /**
     * Queries the buffer's load status. A return of \ref BufferLoad_Pending
     * indicates the buffer is not finished loading and can't be used with a
     * call to \ref Source::play. Buffers created with \ref Context::getBuffer
     * will always return \ref BufferLoad_Ready.
     */
    virtual BufferLoadStatus getLoadStatus() = 0;

    /** Queries if the buffer is in use and can't be removed. */
    virtual bool isInUse() const = 0;
};


class Source {
public:
    /**
     * Plays the source using \param buffer. The same buffer may be played from
     * multiple sources simultaneously.
     */
    virtual void play(Buffer *buffer) = 0;
    /**
     * Plays the source by streaming audio from \param decoder. This will use
     * \param queuelen buffers, each with \param updatelen sample frames. The
     * given decoder must *NOT* have its read or seek methods called from
     * elsewhere while in use.
     */
    virtual void play(SharedPtr<Decoder> decoder, ALuint updatelen, ALuint queuesize) = 0;
    /**
     * Stops playback, releasing the buffer or decoder reference.
     */
    virtual void stop() = 0;

    /** Pauses the source if it is playing. */
    virtual void pause() = 0;

    /** Resumes the source if it is paused. */
    virtual void resume() = 0;

    /** Specifies if the source is currently playing. */
    virtual bool isPlaying() const = 0;

    /** Specifies if the source is currently paused. */
    virtual bool isPaused() const = 0;

    /**
     * Specifies the source's playback priority. Lowest priority sources will
     * be evicted first when higher priority sources are played.
     */
    virtual void setPriority(ALuint priority) = 0;
    /** Retrieves the source's priority. */
    virtual ALuint getPriority() const = 0;

    /**
     * Sets the source's offset, in sample frames. If the source is playing or
     * paused, it will go to that offset immediately, otherwise the source will
     * start at the specified offset the next time it's played.
     */
    virtual void setOffset(uint64_t offset) = 0;
    /**
     * Retrieves the source offset in sample frames. For streaming sources,
     * this will be the offset from the beginning of the stream based on the
     * decoder's reported position.
     *
     * \param latency If non-NULL and the device supports it, the source's
     * latency, in nanoseconds, will be written to that location.
     */
    virtual uint64_t getOffset(uint64_t *latency=0) const = 0;

    virtual void setLooping(bool looping) = 0;
    virtual bool getLooping() const = 0;

    virtual void setPitch(ALfloat pitch) = 0;
    virtual ALfloat getPitch() const = 0;

    virtual void setGain(ALfloat gain) = 0;
    virtual ALfloat getGain() const = 0;

    virtual void setGainRange(ALfloat mingain, ALfloat maxgain) = 0;
    virtual ALfloat getMinGain() const = 0;
    virtual ALfloat getMaxGain() const = 0;

    virtual void setDistanceRange(ALfloat refdist, ALfloat maxdist) = 0;
    virtual ALfloat getReferenceDistance() const = 0;
    virtual ALfloat getMaxDistance() const = 0;

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setPosition(const ALfloat *pos) = 0;
    virtual Vector3 getPosition() const = 0;

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setVelocity(const ALfloat *vel) = 0;
    virtual Vector3 getVelocity() const = 0;

    virtual void setDirection(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setDirection(const ALfloat *dir) = 0;
    virtual Vector3 getDirection() const = 0;

    virtual void setOrientation(ALfloat x1, ALfloat y1, ALfloat z1, ALfloat x2, ALfloat y2, ALfloat z2) = 0;
    virtual void setOrientation(const ALfloat *at, const ALfloat *up) = 0;
    virtual void setOrientation(const ALfloat *ori) = 0;

    virtual void setConeAngles(ALfloat inner, ALfloat outer) = 0;
    virtual ALfloat getInnerConeAngle() const = 0;
    virtual ALfloat getOuterConeAngle() const = 0;

    virtual void setOuterConeGains(ALfloat gain, ALfloat gainhf=1.0f) = 0;
    virtual ALfloat getOuterConeGain() const = 0;
    virtual ALfloat getOuterConeGainHF() const = 0;

    virtual void setRolloffFactors(ALfloat factor, ALfloat roomfactor=0.0f) = 0;
    virtual ALfloat getRolloffFactor() const = 0;
    virtual ALfloat getRoomRolloffFactor() const = 0;

    virtual void setDopplerFactor(ALfloat factor) = 0;
    virtual ALfloat getDopplerFactor() const = 0;

    virtual void setRelative(bool relative) = 0;
    virtual bool getRelative() const = 0;

    virtual void setAirAbsorptionFactor(ALfloat factor) = 0;
    virtual ALfloat getAirAbsorptionFactor() const = 0;

    virtual void setGainAuto(bool directhf, bool send, bool sendhf) = 0;
    virtual bool getDirectGainHFAuto() const = 0;
    virtual bool getSendGainAuto() const = 0;
    virtual bool getSendGainHFAuto() const = 0;

    virtual void setDirectFilter(const FilterParams &filter) = 0;
    virtual void setSendFilter(ALuint send, const FilterParams &filter) = 0;
    virtual void setAuxiliarySend(AuxiliaryEffectSlot *slot, ALuint send) = 0;
    virtual void setAuxiliarySendFilter(AuxiliaryEffectSlot *slot, ALuint send, const FilterParams &filter) = 0;

    /**
     * Updates the source, ensuring that streaming buffers are kept full and
     * resources are released when playback is finished.
     */
    virtual void update() = 0;

    /**
     * Releases the source, stopping playback, releasing resources, and
     * returning it to the system.
     */
    virtual void release() = 0;
};


class AuxiliaryEffectSlot {
public:
    virtual void setGain(ALfloat gain) = 0;
    virtual void setSendAuto(bool sendauto) = 0;

    virtual void applyEffect(const Effect *effect) = 0;

    virtual void release() = 0;

    virtual bool isInUse() const = 0;
};


class Effect {
public:
    virtual void setReverbProperties(const EFXEAXREVERBPROPERTIES &props) = 0;

    virtual void destroy() = 0;
};


/**
 * Audio decoder interface. Applications may derive from this, implementing the
 * necessary methods, and use it in places the API wants a Decoder object.
 */
class Decoder {
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
     * used to load a \ref Buffer.
     */
    virtual uint64_t getLength() = 0;
    /**
     * Retrieves the current sample frame position (i.e. the number of sample
     * frames from the beginning).
     */
    virtual uint64_t getPosition() = 0;
    /**
     * Seek to \param pos, specified in sample frames. Returns true if the seek
     * was successful.
     */
    virtual bool seek(uint64_t pos) = 0;

    /**
     * Retrieves the loop points, in sample frames, as a [start,end) pair. If
     * start >= end, use all available data.
     */
    virtual std::pair<uint64_t,uint64_t> getLoopPoints() const = 0;

    /**
     * Decodes \param count sample frames, writing them to \param ptr, and
     * returns the number of sample frames written. Returning less than the
     * requested count indicates the end of the audio.
     */
    virtual ALuint read(ALvoid *ptr, ALuint count) = 0;
};

/**
 * Audio decoder factory interface. Applications may derive from this,
 * implementing the necessary methods, and use it in places the API wants a
 * DecoderFactory object.
 */
class DecoderFactory {
public:
    virtual ~DecoderFactory() { }

    /**
     * Creates and returns a \ref Decoder instance for the given resource
     * \param file. Returns NULL if a decoder can't be created from the file.
     */
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) = 0;
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
void RegisterDecoder(const std::string &name, SharedPtr<DecoderFactory> factory);

/**
 * Unregisters a decoder factory by name. Alure gives a reference to the
 * instance back to the application and releases its own.
 *
 * \param name The unique name identifying a previously-registered decoder
 * factory.
 *
 * \return The unregistered decoder factory instance, or 0 (nullptr) if a
 * decoder factory with the given name doesn't exist.
 */
SharedPtr<DecoderFactory> UnregisterDecoder(const std::string &name);


/**
 * A file I/O factory interface. Applications may derive from this and set an
 * instance to be used by the audio decoders. By default, the library uses
 * standard I/O.
 */
class FileIOFactory {
public:
    /**
     * Sets the \param factory instance to be used by the audio decoders. If a
     * previous factory was set, it's returned to the application. Passing in a
     * NULL factory reverts to the default.
     */
    static SharedPtr<FileIOFactory> set(SharedPtr<FileIOFactory> factory);

    /**
     * Gets the current FileIOFactory instance being used by the audio
     * decoders.
     */
    static FileIOFactory &get();

    virtual ~FileIOFactory() { }

    /** Opens a read-only binary file for the given \param name. */
    virtual SharedPtr<std::istream> openFile(const std::string &name) = 0;
};


/**
 * A message handler interface. Applications may derive from this and set an
 * instance on a context to receive messages.
 */
class MessageHandler {
public:
    virtual ~MessageHandler() { }

    /**
     * Called when a new buffer is about to be created and loaded. May be
     * called asynchronously for buffers being loaded asynchronously.
     *
     * \param name The resource name, as passed to \ref Context::getBuffer.
     * \param channels Channel configuration of the given audio data.
     * \param type Sample type of the given audio data.
     * \param samplerate Sample rate of the given audio data.
     * \param data The audio data that is about to be fed to the OpenAL buffer.
     */
    virtual void bufferLoading(const std::string &name, ChannelConfig channels, SampleType type, ALuint samplerate, const std::vector<ALbyte> &data) = 0;

    /**
     * Called when a resource isn't found, allowing the app to substitute in a
     * different resource. For buffers created with \ref Context::getBuffer or
     * \ref Context::getBufferAsync, the original name will still be used for
     * the cache map so the app doesn't have to keep track of substituted
     * resource names.
     *
     * This will be called again if the new name isn't found.
     *
     * \param name The resource name that was not found.
     * \param newname The string to write in a resplacement resource name.
     * \return True to look for a replacement resource, false to fail.
     */
    virtual bool resourceNotFound(const std::string &name, std::string &newname) = 0;
};

} // namespace alure

#endif /* AL_ALURE2_H */
