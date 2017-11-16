#ifndef AUXEFFECTSLOT_H
#define AUXEFFECTSLOT_H

#include <algorithm>

#include "main.h"

namespace alure {

class AuxiliaryEffectSlotImpl {
    ContextImpl *const mContext;
    ALuint mId;

    Vector<SourceSend> mSourceSends;

public:
    AuxiliaryEffectSlotImpl(ContextImpl *context, ALuint id)
      : mContext(context), mId(id)
    { }

    void addSourceSend(SourceSend source_send);
    void removeSourceSend(SourceSend source_send);

    ContextImpl *getContext() { return mContext; }
    const ALuint &getId() const { return mId; }

    void setGain(ALfloat gain);
    void setSendAuto(bool sendauto);

    void applyEffect(Effect effect);

    void release();

    Vector<SourceSend> getSourceSends() const { return mSourceSends; }

    bool isInUse() const { return (mSourceSends.size() > 0); }
};

} // namespace alure

#endif /* AUXEFFECTSLOT_H */
