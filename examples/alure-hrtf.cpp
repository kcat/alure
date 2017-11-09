/*
 * An example showing how to enable HRTF rendering, using the ALC_SOFT_HRTF
 * extension.
 */

#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager devMgr = alure::DeviceManager::get();

    int fileidx = 1;
    alure::Device dev;
    if(argc > 3 && strcmp(argv[1], "-device") == 0)
    {
        fileidx = 3;
        dev = devMgr.openPlayback(argv[2], std::nothrow);
        if(!dev)
            std::cerr<< "Failed to open \""<<argv[2]<<"\" - trying default" <<std::endl;
    }
    if(!dev)
        dev = devMgr.openPlayback();
    std::cout<< "Opened \""<<dev.getName()<<"\"" <<std::endl;

    if(!dev.queryExtension("ALC_SOFT_HRTF"))
    {
        std::cerr<< "ALC_SOFT_HRTF not supported!" <<std::endl;
        return 1;
    }

    // Enumerate (and display) the available HRTFs
    std::cout<< "Available HRTFs:\n";
    alure::Vector<alure::String> hrtf_names = dev.enumerateHRTFNames();
    for(const alure::String &name : hrtf_names)
        std::cout<< "    "<<name <<'\n';
    std::cout.flush();

    alure::Vector<alure::AttributePair> attrs;
    attrs.push_back({ALC_HRTF_SOFT, ALC_TRUE});
    if(argc-fileidx > 1 && alure::StringView("-hrtf") == argv[fileidx])
    {
        // Find the given HRTF and add it to the attributes list
        const char *hrtf_name = argv[fileidx+1];
        fileidx += 2;

        auto iter = std::find(hrtf_names.begin(), hrtf_names.end(), hrtf_name);
        if(iter == hrtf_names.end())
            std::cerr<< "HRTF \""<<hrtf_name<<"\" not found" <<std::endl;
        else
            attrs.push_back({ALC_HRTF_ID_SOFT, std::distance(hrtf_names.begin(), iter)});
    }
    attrs.push_back(alure::AttributesEnd());
    alure::Context ctx = dev.createContext(attrs);
    alure::Context::MakeCurrent(ctx);

    std::cout<< "Using HRTF \""<<dev.getCurrentHRTF()<<"\"" <<std::endl;

    for(int i = fileidx;i < argc;i++)
    {
        if(argc-i > 1 && alure::StringView("-hrtf") == argv[i])
        {
            // Find the given HRTF and reset the device using it
            auto iter = std::find(hrtf_names.begin(), hrtf_names.end(), argv[i+1]);
            if(iter == hrtf_names.end())
                std::cerr<< "HRTF \""<<argv[i+1]<<"\" not found" <<std::endl;
            else
            {
                alure::Array<alure::AttributePair,3> attrs{{
                    {ALC_HRTF_SOFT, ALC_TRUE},
                    {ALC_HRTF_ID_SOFT, std::distance(hrtf_names.begin(), iter)},
                    alure::AttributesEnd()
                }};
                dev.reset(attrs);
                std::cout<< "Using HRTF \""<<dev.getCurrentHRTF()<<"\"" <<std::endl;
            }

            ++i;
            continue;
        }

        alure::SharedPtr<alure::Decoder> decoder(ctx.createDecoder(argv[i]));
        alure::Source source = ctx.createSource();

        source.play(decoder, 12000, 4);
        std::cout<< "Playing "<<argv[i]<<" ("<<alure::GetSampleTypeName(decoder->getSampleType())<<", "
                                             <<alure::GetChannelConfigName(decoder->getChannelConfig())<<", "
                                             <<decoder->getFrequency()<<"hz)" <<std::endl;

        float invfreq = 1.0f / decoder->getFrequency();
        while(source.isPlaying())
        {
            std::cout<< "\r "<<std::fixed<<std::setprecision(2)<<
                        source.getSecOffset().count()<<" / "<<(decoder->getLength()*invfreq);
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            ctx.update();
        }
        std::cout<<std::endl;

        source.release();
    }

    alure::Context::MakeCurrent(nullptr);
    ctx.destroy();
    dev.close();

    return 0;
}
