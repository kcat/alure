
#include "buffer.h"

#include <stdexcept>

#include "al.h"

#include "context.h"

namespace alure
{

void ALBuffer::cleanup()
{
    if(mRefs.load() != 0)
        throw std::runtime_error("Buffer is in use");

    alGetError();
    alDeleteBuffers(1, &mId);
    if(alGetError() != AL_NO_ERROR)
        throw std::runtime_error("Buffer failed to delete");
    mId = 0;

    delete this;
}

ALuint ALBuffer::getSize()
{
    CheckContext(mContext);

    ALint size = -1;
    alGetBufferi(mId, AL_SIZE, &size);
    if(size < 0)
        throw std::runtime_error("Buffer size error");
    return size;
}

}
