#ifndef EFFECT_H
#define EFFECT_H

#include "main.h"

namespace alure {

class ContextImpl;

class EffectImpl {
    ContextImpl *const mContext;
    ALuint mId;
    ALenum mType;

public:
    EffectImpl(ContextImpl *context, ALuint id) : mContext(context), mId(id), mType(AL_NONE)
    { }

    void setReverbProperties(const EFXEAXREVERBPROPERTIES &props);

    void destroy();

    ContextImpl *getContext() const { return mContext; }
    ALuint getId() const { return mId; }
};

} // namespace alure

#endif /* EFFECT_H */
