#ifndef AL_ALURE2_H
#define AL_ALURE2_H

#include <vector>
#include <string>
#include <memory>

#include "alc.h"
#include "al.h"

namespace alure {

class DeviceManager;
class Device;
class Context;
class Listener;
class Buffer;
class Source;
class Decoder;
class DecoderFactory;


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
    virtual bool queryExtension(const char *extname) = 0;

    /** Enumerates available device names of the given \param type. */
    virtual std::vector<std::string> enumerate(DeviceEnumeration type) = 0;
    /** Retrieves the default device of the given \param type. */
    virtual std::string defaultDeviceName(DefaultDeviceType type) = 0;

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
    virtual std::string getName(PlaybackDeviceType type) = 0;
    /** Queries the existence of an ALC extension on this device. */
    virtual bool queryExtension(const char *extname) = 0;

    /**
     * Retrieves the ALC version supported by this device, as constructed by
     * \ref MakeVersion.
     */
    virtual ALCuint getALCVersion() = 0;

    /**
     * Retrieves the EFX version supported by this device, as constructed by
     * \ref MakeVersion. If the ALC_EXT_EFX extension is unsupported, this
     * will be 0.
     */
    virtual ALCuint getEFXVersion() = 0;

    /** Retrieves the device's playback frequency, in hz. */
    virtual ALCuint getFrequency() = 0;

    /**
     * Creates a new \ref Context on this device, using the specified
     * \param attributes.
     */
    virtual Context *createContext(ALCint *attributes=0) = 0;

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
     * Creates a \ref Decoder instance for the given audio file or resource
     * \param name. The caller is responsible for deleting the returned object.
     */
    virtual Decoder *createDecoder(const std::string &name) = 0;

    /**
     * Retrieves a \ref Listener instance for this context. Each context will
     * only have one listener.
     */
    virtual Listener *getListener() = 0;

    // Functions below require the context to be current

    /**
     * Creates and caches a \ref Buffer for the given audio file or resource
     * \param name. Multiple calls with the same name will return the same
     * \ref Buffer object.
     */
    virtual Buffer *getBuffer(const std::string &name) = 0;

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
    /** Finalizes \param source, stopping it and returning it to the system. */
    virtual void finalize(Source *source) = 0;

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

enum SampleConfig {
    SampleConfig_Mono,
    SampleConfig_Stereo,
    SampleConfig_Rear,
    SampleConfig_Quad,
    SampleConfig_X51,
    SampleConfig_X61,
    SampleConfig_X71,
    SampleConfig_BFmt_WXY,
    SampleConfig_BFmt_WXYZ
};
const char *GetSampleConfigName(SampleConfig cfg);


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
    /** Retrieves the length of the buffer in sample frames. */
    virtual ALuint getLength() const = 0;

    /** Retrieves the buffer's frequency in hz. */
    virtual ALuint getFrequency() const = 0;

    /** Retrieves the buffer's sample configuration. */
    virtual SampleConfig getSampleConfig() const = 0;

    /** Retrieves the buffer's sample type. */
    virtual SampleType getSampleType() const = 0;

    /** Retrieves the storage size used by the buffer, in bytes. */
    virtual ALuint getSize() const = 0;

    /** Queries if the buffer is not in use and can be removed. */
    virtual bool isRemovable() const = 0;
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
     * given decoder must *NOT* be in use elsewhere, and must not be deleted
     * while playing.
     */
    virtual void play(Decoder *decoder, ALuint updatelen, ALuint queuesize) = 0;
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
     * Sets the source's offset, in sample frames. If the source is playing it
     * will go to that offset immediately, otherwise the source will start at
     * the specified offset the next time it's played or resumed.
     */
    virtual void setOffset(ALuint offset) = 0;
    /**
     * Retrieves the source offset in sample frames. For streaming sources,
     * this will be the offset from the beginning of the stream based on the
     * decoder's reported position.
     *
     * \param latency If non-NULL and the device supports it, the source's, in
     * nanoseconds, will be written to that location.
     */
    virtual ALuint getOffset(uint64_t *latency=0) const = 0;

    virtual void setLooping(bool looping) = 0;
    virtual bool getLooping() const = 0;

    virtual void setPitch(ALfloat pitch) = 0;

    virtual void setGain(ALfloat gain) = 0;
    virtual void setGainRange(ALfloat mingain, ALfloat maxgain) = 0;

    virtual void setDistanceRange(ALfloat refdist, ALfloat maxdist) = 0;

    virtual void setPosition(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setPosition(const ALfloat *pos) = 0;

    virtual void setVelocity(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setVelocity(const ALfloat *vel) = 0;

    virtual void setDirection(ALfloat x, ALfloat y, ALfloat z) = 0;
    virtual void setDirection(const ALfloat *dir) = 0;

    virtual void setConeAngles(ALfloat inner, ALfloat outer) = 0;
    virtual void setOuterConeGain(ALfloat gain) = 0;

    virtual void setRolloffFactor(ALfloat factor) = 0;

    virtual void setDopplerFactor(ALfloat factor) = 0;

    /**
     * Updates the source, ensuring that streaming buffers are kept full and
     * resources are released when playback is finished.
     */
    virtual void update() = 0;
};


/**
 * Audio decoder interface. Applications may derive from this, implementing the
 * necessary methods, and use it in places the API wants a Decoder object.
 */
class Decoder {
public:
    virtual ~Decoder() { }

    /** Retrieves the sample frequency, in hz, of the audio being decoded. */
    virtual ALuint getFrequency() = 0;
    /** Retrieves the channel configuration of the audio being decoded. */
    virtual SampleConfig getSampleConfig() = 0;
    /** Retrieves the sample type of the audio being decoded. */
    virtual SampleType getSampleType() = 0;

    /**
     * Retrieves the total length of the audio, in sample frames. If unknown,
     * returns 0. Note that if the returned length is 0, the decoder may not be
     * used to load a \ref Buffer.
     */
    virtual ALuint getLength() = 0;
    /**
     * Retrieves the current sample frame position (i.e. the number of sample
     * frames from the beginning).
     */
    virtual ALuint getPosition() = 0;
    /**
     * Seek as close as possible to \param pos, specified in sample frames.
     * Returns true if the seek was successful.
     */
    virtual bool seek(ALuint pos) = 0;

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
     * Creates and returns a \ref Decoder instance for the specified audio
     * resource \param name. The name will be the string specified to the
     * Context's createDecoder or getBuffer methods.
     */
    virtual Decoder *createDecoder(const std::string &name) = 0;
};

/**
 * Registers a decoder factory for decoding audio. Registered factories are
 * used on a last-registered basis, e.g. if Factory1 is registered, then
 * Factory2 is registered, Factory2 will be used before Factory1.
 *
 * \param name A unique name identifying this decoder factory.
 * \param factory A DecoderFactory instance used to create Decoder instances.
 * The library takes ownership of the factory instance, and will delete it
 * automatically at program termination.
 */
void RegisterDecoder(const std::string &name, DecoderFactory *factory);

/**
 * Unregisters a decoder factory by name. Ownership of the DecoderFactory
 * instance is returned to the application.
 *
 * \param name The unique name identifying a previously-registered decoder
 * factory.
 *
 * \return The unregistered decoder factory instance, or 0 (nullptr) if a
 * decoder factory with the given name doesn't exist.
 */
DecoderFactory *UnregisterDecoder(const std::string &name);


/**
 * A file I/O factory interface. Applications may derive from this and set an
 * instance to be used by the built-in audio decoders.
 */
class FileIOFactory {
public:
    /**
     * Sets the \param factory instance to be used by the built-in audio
     * decoders. The library takes ownership of the factory and will delete it
     * at program termination. If a previous factory was set, it and ownership
     * to it are returned to the application. Passing in a NULL factory reverts
     * to the default.
     */
    static FileIOFactory *set(FileIOFactory *factory);
    /**
     * Gets the current FileIOFactory instance being used by the built-in audio
     * decoders. The returned object must NOT be deleted.
     */
    static FileIOFactory *get();

    virtual ~FileIOFactory() { }

    /**
     * Creates a read-only binary file instance for the given \param name.
     */
    virtual std::auto_ptr<std::istream> createFile(const std::string &name) = 0;
};

} // namespace alure

#endif /* AL_ALURE2_H */
