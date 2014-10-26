#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <time.h>
#include <stdint.h>
inline void Sleep(uint32_t ms)
{
    struct timespec ts, rem;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    while(nanosleep(&ts, &rem) == -1 && errno == EINTR)
        ts = rem;
}
#endif

#include <iostream>
#include <iomanip>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager *devMgr = alure::DeviceManager::get();

    alure::Device *dev = devMgr->openPlayback();
    std::cout<< "Opened \""<<dev->getName(alure::PlaybackDevType_Basic)<<"\"" <<std::endl;

    alure::Context *ctx = dev->createContext();
    alure::Context::MakeCurrent(ctx);

    for(int i = 1;i < argc;i++)
    {
        alure::Buffer *buffer = ctx->getBuffer(argv[i]);
        alure::Source *source = ctx->getSource();
        source->play(buffer);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(buffer->getSampleType())<<", "
                                             <<alure::GetSampleConfigName(buffer->getSampleConfig())<<", "
                                             <<buffer->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / buffer->getFrequency();
        while(source->isPlaying())
        {
            std::cout<< "\r "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<
                        (source->getOffset()*invfreq)<<" / "<<(buffer->getLength()*invfreq);
            std::cout.flush();
            Sleep(25);
            ctx->update();
        }
        std::cout<<std::endl;

        ctx->finalize(source);
        source = 0;
        ctx->removeBuffer(buffer);
        buffer = 0;
    }

    alure::Context::MakeCurrent(0);
    ctx->destroy();
    ctx = 0;
    dev->close();
    dev = 0;

    return 0;
}
