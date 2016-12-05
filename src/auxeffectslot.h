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

    void addSourceSend(Source *source, ALuint send)
    { mSourceSends.emplace_back((SourceSend){source, send}); }
    void removeSourceSend(Source *source, ALuint send)
    {
        auto iter = std::find(mSourceSends.cbegin(), mSourceSends.cend(), SourceSend{source, send});
        if(iter != mSourceSends.cend()) mSourceSends.erase(iter);
    }

    ALContext *getContext() { return mContext; }
    const ALuint &getId() const { return mId; }

    void setGain(ALfloat gain) override final;
    void setSendAuto(bool sendauto) override final;

    void applyEffect(const Effect *effect) override final;

    void release() override final;

    Vector<SourceSend> getSourceSends() const override final { return mSourceSends; }

    bool isInUse() const override final { return (mSourceSends.size() > 0); }
};

} // namespace alure

#endif /* AUXEFFECTSLOT_H */
