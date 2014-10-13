
#include <iostream>

#include "alure2.h"

int main()
{
    alure::DeviceManager *devMgr = alure::DeviceManager::get();
    std::vector<std::string> list;

    std::cout<< "Available basic devices:" <<std::endl;
    list = devMgr->enumerate(alure::DevEnum_Basic);
    for(const auto &name : list)
        std::cout<< "  "<<name <<std::endl;

    std::cout<< std::endl<<"Available devices:" <<std::endl;
    list = devMgr->enumerate(alure::DevEnum_Complete);
    for(const auto &name : list)
        std::cout<< "  "<<name <<std::endl;

    std::cout<< std::endl<<"Available capture devices:" <<std::endl;
    list = devMgr->enumerate(alure::DevEnum_Capture);
    for(const auto &name : list)
        std::cout<< "  "<<name <<std::endl;

    return 0;
}
