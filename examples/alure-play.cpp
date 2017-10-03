/*
 * A simple example showing how to load and play a sound.
 */

#include <string.h>

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager &devMgr = alure::DeviceManager::get();

    int fileidx = 1;
    alure::Device dev = [argc,argv,&devMgr,&fileidx]() -> alure::Device
    {
        if(argc > 3 && strcmp(argv[1], "-device") == 0)
        {
            fileidx = 3;
            try {
                return devMgr.openPlayback(argv[2]);
            }
            catch(...) {
                std::cerr<< "Failed to open \""<<argv[2]<<"\" - trying default" <<std::endl;
            }
        }
        return devMgr.openPlayback();
    }();
    std::cout<< "Opened \""<<dev.getName()<<"\"" <<std::endl;

    alure::Context ctx = dev.createContext();
    alure::Context::MakeCurrent(ctx);

    for(int i = fileidx;i < argc;i++)
    {
        alure::Buffer *buffer = ctx.getBuffer(argv[i]);
        alure::Source *source = ctx.createSource();
        source->play(buffer);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(buffer->getSampleType())<<", "
                                             <<alure::GetChannelConfigName(buffer->getChannelConfig())<<", "
                                             <<buffer->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / buffer->getFrequency();
        while(source->isPlaying())
        {
            std::cout<< "\r "<<std::setiosflags(std::ios::fixed)<<std::setprecision(2)<<
                        (source->getOffset()*invfreq)<<" / "<<(buffer->getLength()*invfreq);
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            ctx.update();
        }
        std::cout<<std::endl;

        source->release();
        source = 0;
        ctx.removeBuffer(buffer);
        buffer = 0;
    }

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
