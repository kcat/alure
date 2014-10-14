
#include <iostream>

#include "alure2.h"

int main()
{
    alure::DeviceManager *devMgr = alure::DeviceManager::get();
    std::vector<std::string> list;
    std::string defname;

    list = devMgr->enumerate(alure::DevEnum_Basic);
    defname = devMgr->defaultDeviceName(alure::DefaultDevType_Basic);
    std::cout<< "Available basic devices:" <<std::endl;
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<std::endl;

    defname = devMgr->defaultDeviceName(alure::DefaultDevType_Complete);
    list = devMgr->enumerate(alure::DevEnum_Complete);
    std::cout<< std::endl<<"Available devices:" <<std::endl;
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<std::endl;

    list = devMgr->enumerate(alure::DevEnum_Capture);
    defname = devMgr->defaultDeviceName(alure::DefaultDevType_Capture);
    std::cout<< std::endl<<"Available capture devices:" <<std::endl;
    for(const auto &name : list)
        std::cout<< "  "<<name<<((defname==name)?"  [DEFAULT]":"") <<std::endl;

    return 0;
}
