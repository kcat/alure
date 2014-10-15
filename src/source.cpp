
#include "source.h"

#include <stdexcept>

#include "al.h"

#include "context.h"
#include "buffer.h"

namespace alure
{

ALSource::~ALSource()
{
    if(mId)
        mContext->insertSourceId(mId);
    mId = 0;
    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;
}

void ALSource::finalize()
{
    CheckContext(mContext);
    if(mId)
    {
        mContext->insertSourceId(mId);
        alSourceStop(mId);
        alSourcei(mId, AL_BUFFER, 0);
        mId = 0;
    }
    mContext->insertSource(this);
    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;
}


void ALSource::play(Buffer *buffer, float volume)
{
    ALBuffer *albuf = dynamic_cast<ALBuffer*>(buffer);
    if(!albuf) throw std::runtime_error("Buffer is not valid");
    if(volume < 0.0f || volume > 1.0f)
        throw std::runtime_error("Volume out of range");
    CheckContext(mContext);
    CheckContextDevice(albuf->getDevice());

    if(!mId)
        mId = mContext->getSourceId();
    else
    {
        alSourceStop(mId);
        alSourcei(mId, AL_BUFFER, 0);
    }

    albuf->addRef();
    if(mBuffer)
        mBuffer->decRef();
    mBuffer = albuf;

    alSourcei(mId, AL_BUFFER, mBuffer->getId());
    alSourcef(mId, AL_GAIN, volume);
    alSourcePlay(mId);
}

void ALSource::stop()
{
    CheckContext(mContext);
    if(mId)
    {
        mContext->insertSourceId(mId);
        alSourceStop(mId);
        alSourcei(mId, AL_BUFFER, 0);
        mId = 0;
    }
    if(mBuffer)
        mBuffer->decRef();
    mBuffer = 0;
}

bool ALSource::isPlaying() const
{
    CheckContext(mContext);
    if(!mId) return false;

    ALint state = -1;
    alGetSourcei(mId, AL_SOURCE_STATE, &state);
    if(state == -1)
        throw std::runtime_error("Source state error");
    return state == AL_PLAYING;
}

}
