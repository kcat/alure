#ifndef AUXEFFECTSLOT_H
#define AUXEFFECTSLOT_H

#include <algorithm>

#include "main.h"

#include "al.h"

#include "refcount.h"

namespace alure {

class ALContext;

inline bool operator==(const SourceSend &lhs, const SourceSend &rhs)
{ return lhs.mSource == rhs.mSource && lhs.mSend == rhs.mSend; }

class ALAuxiliaryEffectSlot : public AuxiliaryEffectSlot {
    ALContext *const mContext;
    ALuint mId;

    Vector<SourceSend> mSourceSends;

public:
    ALAuxiliaryEffectSlot(ALContext *context, ALuint id)
      : mContext(context), mId(id)
    { }
    virtual ~ALAuxiliaryEffectSlot() { }

    void addSourceSend(Source *source, ALuint send) { mSourceSends.push_back({source, send}); }
    void removeSourceSend(Source *source, ALuint send)
    {
        auto iter = std::find(mSourceSends.cbegin(), mSourceSends.cend(), SourceSend{source, send});
        if(iter != mSourceSends.cend()) mSourceSends.erase(iter);
    }

    ALContext *getContext() { return mContext; }
    const ALuint &getId() const { return mId; }

    virtual void setGain(ALfloat gain) final;
    virtual void setSendAuto(bool sendauto) final;

    virtual void applyEffect(const Effect *effect) final;

    virtual void release() final;

    virtual Vector<SourceSend> getSourceSends() const final { return mSourceSends; }

    virtual bool isInUse() const final { return (mSourceSends.size() > 0); }
};

} // namespace alure

#endif /* AUXEFFECTSLOT_H */
