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

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

#include "alure2.h"
#include "alext.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager *devMgr = alure::DeviceManager::get();

    alure::Device *dev = devMgr->openPlayback();
    std::cout<< "Opened \""<<dev->getName(alure::PlaybackDevType_Basic)<<"\"" <<std::endl;

    if(!dev->queryExtension("ALC_SOFT_HRTF"))
    {
        std::cerr<< "ALC_SOFT_HRTF not supported!" <<std::endl;
        return 1;
    }

    // Enumerate (and display) the available HRTFs
    std::cout<< "Available HRTFs:" <<std::endl;
    std::vector<std::string> hrtf_names = dev->enumerateHRTFNames();
    for(const std::string &name : hrtf_names)
        std::cout<< "    "<<name <<std::endl;

    int i = 1;
    std::vector<ALCint> attrs;
    attrs.push_back(ALC_HRTF_SOFT);
    attrs.push_back(ALC_TRUE);
    if(argc-i > 1 && strcasecmp(argv[i], "-hrtf") == 0)
    {
        // Find the given HRTF and add it to the attributes list
        auto iter = std::find_if(hrtf_names.begin(), hrtf_names.end(),
            [argv, i](const std::string &name) -> bool
            { return name == argv[i+1]; }
        );
        if(iter == hrtf_names.end())
            std::cerr<< "HRTF \""<<argv[i+1]<<"\" not found" <<std::endl;
        else
        {
            attrs.push_back(ALC_HRTF_ID_SOFT);
            attrs.push_back(std::distance(hrtf_names.begin(), iter));
        }
        i += 2;
    }
    attrs.push_back(0);
    alure::Context *ctx = dev->createContext(attrs.data());
    alure::Context::MakeCurrent(ctx);

    std::cout<< "Using HRTF \""<<dev->getCurrentHRTF()<<"\"" <<std::endl;

    for(;i < argc;i++)
    {
        if(argc-i > 1 && strcasecmp(argv[i], "-hrtf") == 0)
        {
            // Find the given HRTF and reset the device using it
            auto iter = std::find_if(hrtf_names.begin(), hrtf_names.end(),
                [argv, i](const std::string &name) -> bool
                { return name == argv[i+1]; }
            );
            if(iter == hrtf_names.end())
                std::cerr<< "HRTF \""<<argv[i+1]<<"\" not found" <<std::endl;
            else
            {
                attrs.clear();
                attrs.push_back(ALC_HRTF_SOFT);
                attrs.push_back(ALC_TRUE);
                attrs.push_back(ALC_HRTF_ID_SOFT);
                attrs.push_back(std::distance(hrtf_names.begin(), iter));
                dev->reset(attrs.data());

                std::cout<< "Using HRTF \""<<dev->getCurrentHRTF()<<"\"" <<std::endl;
            }

            ++i;
            continue;
        }

        alure::SharedPtr<alure::Decoder> decoder(ctx->createDecoder(argv[i]));
        alure::Source *source = ctx->getSource();

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
