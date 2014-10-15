#ifndef SOURCE_H
#define SOURCE_H

#include "alure2.h"

#include "al.h"

#if __cplusplus < 201103L
#define final
#endif

namespace alure {

class ALContext;
class ALBuffer;
class ALBufferStream;

class ALSource : public Source {
    ALContext *const mContext;
    ALuint mId;

    ALBuffer *mBuffer;
    ALBufferStream *mStream;

    void reset();

public:
    ALSource(ALContext *context)
      : mContext(context), mId(0), mBuffer(0), mStream(0)
    { }

    void finalize();

    virtual void play(Buffer *buffer, float volume) final;
    virtual void play(Decoder *decoder, ALuint updatelen, ALuint queuesize, float volume) final;
    virtual void stop() final;

    virtual bool isPlaying() const final;

    virtual void update() final;
};

} // namespace alure

#endif /* SOURCE_H */
