
#include "source.h"

#include <stdexcept>

#include "al.h"

#include "context.h"
#include "buffer.h"

namespace alure
{

void ALSource::stop()
{
    alSourceStop(mId);
}

}
