#ifndef AUXEFFECTSLOT_H
#define AUXEFFECTSLOT_H

#include "main.h"

#include "al.h"

#include "refcount.h"

namespace alure {

class ALContext;


class ALAuxiliaryEffectSlot : public AuxiliaryEffectSlot {
    ALContext *const mContext;
    ALuint mId;

    RefCount mRefs;

public:
    ALAuxiliaryEffectSlot(ALContext *context, ALuint id)
      : mContext(context), mId(id), mRefs(0)
    { }
    virtual ~ALAuxiliaryEffectSlot() { }

    virtual void setGain(ALfloat gain) final;
    virtual void setSendAuto(bool sendauto) final;

    virtual void applyEffect(const Effect *effect) final;

    virtual void release() final;

    virtual bool isInUse() const final { return (mRefs.load() > 0); }

    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }
    long getRef() { return mRefs.load(); }

    ALContext *getContext() { return mContext; }
    const ALuint &getId() const { return mId; }
};

} // namespace alure

#endif /* AUXEFFECTSLOT_H */
