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

class ALSource : public Source {
    ALContext *const mContext;
    ALuint mId;

    ALBuffer *mBuffer;

public:
    ALSource(ALContext *context, ALuint id, ALBuffer *buffer)
      : mContext(context), mId(id), mBuffer(buffer)
    { }

    virtual void stop() final;
};

} // namespace alure

#endif /* SOURCE_H */
