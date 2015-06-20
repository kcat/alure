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
#include <sstream>
#include <iomanip>
#include <cstring>

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
        float offset = 0.0f;
        if(i+2 < argc && strcmp(argv[i], "-start") == 0)
        {
            std::stringstream sstr(argv[i+1]);
            sstr >> offset;

            i += 2;
        }

        alure::SharedPtr<alure::Decoder> decoder(ctx->createDecoder(argv[i]));
        alure::Source *source = ctx->getSource();

        if(offset > 0.0f)
            source->setOffset(uint64_t(offset * decoder->getFrequency()));

        source->play(decoder, 32768, 4);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(decoder->getSampleType())<<", "
                                             <<alure::GetSampleConfigName(decoder->getSampleConfig())<<", "
                                             <<decoder->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / decoder->getFrequency();
        while(source->isPlaying())
        {
            std::cout<< "\r "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<
                        (source->getOffset()*invfreq)<<" / "<<(decoder->getLength()*invfreq);
            std::cout.flush();
            Sleep(25);
            ctx->update();
        }
        std::cout<<std::endl;

        source->release();
        source = 0;
    }

    alure::Context::MakeCurrent(0);
    ctx->destroy();
    ctx = 0;
    dev->close();
    dev = 0;

    return 0;
}
