
#include "config.h"

#include "auxeffectslot.h"

#include <stdexcept>

#include "al.h"
#include "alext.h"

#include "context.h"
#include "effect.h"

namespace alure
{

void ALAuxiliaryEffectSlot::setGain(ALfloat gain)
{
    if(!(gain >= 0.0f && gain <= 1.0f))
        throw std::runtime_error("Gain out of range");
    CheckContext(mContext);
    mContext->alAuxiliaryEffectSlotf(mId, AL_EFFECTSLOT_GAIN, gain);
}

void ALAuxiliaryEffectSlot::setSendAuto(bool sendauto)
{
    CheckContext(mContext);
    mContext->alAuxiliaryEffectSloti(mId, AL_EFFECTSLOT_AUXILIARY_SEND_AUTO, sendauto ? AL_TRUE : AL_FALSE);
}

void ALAuxiliaryEffectSlot::applyEffect(const Effect *effect)
{
    const ALEffect *eff = cast<const ALEffect*>(effect);
    if(!eff) throw std::runtime_error("Invalid Effect");
    CheckContext(mContext);

    mContext->alAuxiliaryEffectSloti(mId, AL_EFFECTSLOT_EFFECT, eff->getId());
}


void ALAuxiliaryEffectSlot::release()
{
    CheckContext(mContext);
    if(isInUse())
        throw std::runtime_error("AuxiliaryEffectSlot is in use");

    alGetError();
    mContext->alDeleteAuxiliaryEffectSlots(1, &mId);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("AuxiliaryEffectSlot failed to delete");
    mId = 0;

    delete this;
}

} // namespace alure
