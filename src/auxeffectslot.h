#ifndef AUXEFFECTSLOT_H
#define AUXEFFECTSLOT_H

#include "main.h"

#include <algorithm>

namespace alure {

inline bool operator==(const SourceSend &lhs, const SourceSend &rhs)
{ return lhs.mSource == rhs.mSource && lhs.mSend == rhs.mSend; }

class AuxiliaryEffectSlotImpl {
    ContextImpl *const mContext;
    ALuint mId;

    Vector<SourceSend> mSourceSends;

public:
    AuxiliaryEffectSlotImpl(ContextImpl *context, ALuint id)
      : mContext(context), mId(id)
    { }

    void addSourceSend(Source source, ALuint send)
    { mSourceSends.emplace_back((SourceSend){source, send}); }
    void removeSourceSend(Source source, ALuint send)
    {
        auto iter = std::find(mSourceSends.cbegin(), mSourceSends.cend(), SourceSend{source, send});
        if(iter != mSourceSends.cend()) mSourceSends.erase(iter);
    }

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
