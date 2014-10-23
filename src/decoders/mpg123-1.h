#ifndef DECODERS_MPG123_H
#define DECODERS_MPG123_H

#include "alure2.h"

#include "mpg123.h"

namespace alure {

class Mpg123Decoder : public Decoder {
    mpg123_handle *mMpg123;
    int mChannels;
    long mSampleRate;

public:
    Mpg123Decoder(mpg123_handle *mpg123, int chans, long srate);
    virtual ~Mpg123Decoder();

    virtual ALuint getFrequency() final;
    virtual SampleConfig getSampleConfig() final;
    virtual SampleType getSampleType() final;

    virtual ALuint getLength() final;
    virtual ALuint getPosition() final;
    virtual bool seek(ALuint pos) final;

    virtual ALuint read(ALvoid *ptr, ALuint count) final;

    static Decoder *openFile(const std::string &name);
};

} // namespace alure

#endif /* DECODERS_MPG123_H */
