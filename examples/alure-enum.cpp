
#include <iostream>

#include "alure2.h"

int main()
{
    alure::DeviceManager *devMgr = alure::DeviceManager::get();
    alure::Vector<alure::String> list;
    alure::String defname;

    list = devMgr->enumerate(alure::DevEnum_Basic);
    defname = devMgr->defaultDeviceName(alure::DefaultDevType_Basic);
    std::cout<< "Available basic devices:" <<std::endl;
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<std::endl;
    std::cout<<std::endl;

    list = devMgr->enumerate(alure::DevEnum_Complete);
    defname = devMgr->defaultDeviceName(alure::DefaultDevType_Complete);
    std::cout<< "Available devices:" <<std::endl;
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<std::endl;
    std::cout<<std::endl;

    list = devMgr->enumerate(alure::DevEnum_Capture);
    defname = devMgr->defaultDeviceName(alure::DefaultDevType_Capture);
    std::cout<< "Available capture devices:" <<std::endl;
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<std::endl;
    std::cout<<std::endl;

    ALCuint version;
    alure::Device *dev = devMgr->openPlayback();
    std::cout<< "Info for device \""<<dev->getName(alure::PlaybackDevType_Complete)<<"\":" <<std::endl;
    version = dev->getALCVersion();
    std::cout<< "ALC version: "<<alure::MajorVersion(version)<<"."<<alure::MinorVersion(version) <<std::endl;
    version = dev->getEFXVersion();
    if(version)
    {
        std::cout<< "EFX version: "<<alure::MajorVersion(version)<<"."<<alure::MinorVersion(version) <<std::endl;
        std::cout<< "Max auxiliary sends: "<<dev->getMaxAuxiliarySends() <<std::endl;
    }
    else
        std::cout<< "EFX not supported" <<std::endl;
    dev->close();
    dev = 0;

    return 0;
}
