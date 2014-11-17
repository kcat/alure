#ifndef AUXEFFECTSLOT_H
#define AUXEFFECTSLOT_H

#include "alure2.h"

#include "al.h"

#include "refcount.h"

#if __cplusplus < 201103L
#define final
#endif

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

    virtual void setEffect(const Effect *effect) final;

    void cleanup();

    long addRef() { return ++mRefs; }
    long decRef() { return --mRefs; }
    long getRef() { return mRefs.load(); }

    ALContext *getContext() { return mContext; }
    const ALuint &getId() const { return mId; }

    virtual bool isRemovable() const final;
};

} // namespace alure

#endif /* AUXEFFECTSLOT_H */
