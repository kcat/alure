#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager &devMgr = alure::DeviceManager::get();

    alure::Device *dev = devMgr.openPlayback();
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
                                             <<alure::GetChannelConfigName(decoder->getChannelConfig())<<", "
                                             <<decoder->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / decoder->getFrequency();
        while(source->isPlaying())
        {
            std::cout<< "\r "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<
                        (source->getOffset()*invfreq)<<" / "<<(decoder->getLength()*invfreq);
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
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
