/*
 * An example showing how to enable HRTF rendering, using the ALC_SOFT_HRTF
 * extension.
 */

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

#include "alure2.h"

int main(int argc, char *argv[])
{
    alure::DeviceManager devMgr = alure::DeviceManager::getInstance();

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

    // Enumerate (and display) the available HRTFs
    alure::Vector<alure::String> hrtf_names = dev.enumerateHRTFNames();
    if(hrtf_names.empty())
        std::cout<< "No HRTFs found!\n";
    else
    {
        std::cout<< "Available HRTFs:\n";
        for(const alure::String &name : hrtf_names)
            std::cout<< "    "<<name <<'\n';
    }
    std::cout.flush();

    alure::Vector<alure::AttributePair> attrs;
    attrs.push_back({ALC_HRTF_SOFT, ALC_TRUE});
    if(argc-fileidx > 1 && alure::StringView("-hrtf") == argv[fileidx])
    {
        // Find the given HRTF and add it to the attributes list
        alure::StringView hrtf_name = argv[fileidx+1];
        fileidx += 2;

        size_t idx = std::distance(
            hrtf_names.begin(), std::find(hrtf_names.begin(), hrtf_names.end(), hrtf_name)
        );
        if(idx == hrtf_names.size())
            std::cerr<< "HRTF \""<<hrtf_name<<"\" not found" <<std::endl;
        else
            attrs.push_back({ALC_HRTF_ID_SOFT, static_cast<ALint>(idx)});
    }
    attrs.push_back(alure::AttributesEnd());
    alure::Context ctx = dev.createContext(attrs);
    alure::Context::MakeCurrent(ctx);

    if(dev.isHRTFEnabled())
        std::cout<< "Using HRTF \""<<dev.getCurrentHRTF()<<"\"" <<std::endl;
    else
        std::cout<< "HRTF not enabled!" <<std::endl;

    for(int i = fileidx;i < argc;i++)
    {
        if(argc-i > 1 && alure::StringView("-hrtf") == argv[i])
        {
            // Find the given HRTF and reset the device using it
            alure::StringView hrtf_name = argv[i+1];
            size_t idx = std::distance(
                hrtf_names.begin(), std::find(hrtf_names.begin(), hrtf_names.end(), hrtf_name)
            );
            if(idx == hrtf_names.size())
                std::cerr<< "HRTF \""<<hrtf_name<<"\" not found" <<std::endl;
            else
            {
                alure::Array<alure::AttributePair,3> attrs{{
                    {ALC_HRTF_SOFT, ALC_TRUE},
                    {ALC_HRTF_ID_SOFT, static_cast<ALint>(idx)},
                    alure::AttributesEnd()
                }};
                dev.reset(attrs);
                if(dev.isHRTFEnabled())
                    std::cout<< "Using HRTF \""<<dev.getCurrentHRTF()<<"\"" <<std::endl;
                else
                    std::cout<< "HRTF not enabled!" <<std::endl;
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
