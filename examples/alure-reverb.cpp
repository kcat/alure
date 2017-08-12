/*
 * An example showing how to load and apply a reverb effect to a source.
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <thread>
#include <chrono>

#include "alure2.h"

#include "efx-presets.h"

template<typename T, size_t N>
static inline size_t countof(const T(&)[N])
{ return N; }

#define DECL(x) { #x, EFX_REVERB_PRESET_##x }
static const struct {
    const char name[32];
    EFXEAXREVERBPROPERTIES props;
} reverblist[] = {
    DECL(GENERIC),
    DECL(PADDEDCELL),
    DECL(ROOM),
    DECL(BATHROOM),
    DECL(LIVINGROOM),
    DECL(STONEROOM),
    DECL(AUDITORIUM),
    DECL(CONCERTHALL),
    DECL(CAVE),
    DECL(ARENA),
    DECL(HANGAR),
    DECL(CARPETEDHALLWAY),
    DECL(HALLWAY),
    DECL(STONECORRIDOR),
    DECL(ALLEY),
    DECL(FOREST),
    DECL(CITY),
    DECL(MOUNTAINS),
    DECL(QUARRY),
    DECL(PLAIN),
    DECL(PARKINGLOT),
    DECL(SEWERPIPE),
    DECL(UNDERWATER),
    DECL(DRUGGED),
    DECL(DIZZY),
    DECL(PSYCHOTIC),

    DECL(CASTLE_SMALLROOM),
    DECL(CASTLE_SHORTPASSAGE),
    DECL(CASTLE_MEDIUMROOM),
    DECL(CASTLE_LARGEROOM),
    DECL(CASTLE_LONGPASSAGE),
    DECL(CASTLE_HALL),
    DECL(CASTLE_CUPBOARD),
    DECL(CASTLE_COURTYARD),
    DECL(CASTLE_ALCOVE),

    DECL(FACTORY_SMALLROOM),
    DECL(FACTORY_SHORTPASSAGE),
    DECL(FACTORY_MEDIUMROOM),
    DECL(FACTORY_LARGEROOM),
    DECL(FACTORY_LONGPASSAGE),
    DECL(FACTORY_HALL),
    DECL(FACTORY_CUPBOARD),
    DECL(FACTORY_COURTYARD),
    DECL(FACTORY_ALCOVE),

    DECL(ICEPALACE_SMALLROOM),
    DECL(ICEPALACE_SHORTPASSAGE),
    DECL(ICEPALACE_MEDIUMROOM),
    DECL(ICEPALACE_LARGEROOM),
    DECL(ICEPALACE_LONGPASSAGE),
    DECL(ICEPALACE_HALL),
    DECL(ICEPALACE_CUPBOARD),
    DECL(ICEPALACE_COURTYARD),
    DECL(ICEPALACE_ALCOVE),

    DECL(SPACESTATION_SMALLROOM),
    DECL(SPACESTATION_SHORTPASSAGE),
    DECL(SPACESTATION_MEDIUMROOM),
    DECL(SPACESTATION_LARGEROOM),
    DECL(SPACESTATION_LONGPASSAGE),
    DECL(SPACESTATION_HALL),
    DECL(SPACESTATION_CUPBOARD),
    DECL(SPACESTATION_ALCOVE),

    DECL(WOODEN_SMALLROOM),
    DECL(WOODEN_SHORTPASSAGE),
    DECL(WOODEN_MEDIUMROOM),
    DECL(WOODEN_LARGEROOM),
    DECL(WOODEN_LONGPASSAGE),
    DECL(WOODEN_HALL),
    DECL(WOODEN_CUPBOARD),
    DECL(WOODEN_COURTYARD),
    DECL(WOODEN_ALCOVE),

    DECL(SPORT_EMPTYSTADIUM),
    DECL(SPORT_SQUASHCOURT),
    DECL(SPORT_SMALLSWIMMINGPOOL),
    DECL(SPORT_LARGESWIMMINGPOOL),
    DECL(SPORT_GYMNASIUM),
    DECL(SPORT_FULLSTADIUM),
    DECL(SPORT_STADIUMTANNOY),

    DECL(PREFAB_WORKSHOP),
    DECL(PREFAB_SCHOOLROOM),
    DECL(PREFAB_PRACTISEROOM),
    DECL(PREFAB_OUTHOUSE),
    DECL(PREFAB_CARAVAN),

    DECL(DOME_TOMB),
    DECL(PIPE_SMALL),
    DECL(DOME_SAINTPAULS),
    DECL(PIPE_LONGTHIN),
    DECL(PIPE_LARGE),
    DECL(PIPE_RESONANT),

    DECL(OUTDOORS_BACKYARD),
    DECL(OUTDOORS_ROLLINGPLAINS),
    DECL(OUTDOORS_DEEPCANYON),
    DECL(OUTDOORS_CREEK),
    DECL(OUTDOORS_VALLEY),

    DECL(MOOD_HEAVEN),
    DECL(MOOD_HELL),
    DECL(MOOD_MEMORY),

    DECL(DRIVING_COMMENTATOR),
    DECL(DRIVING_PITGARAGE),
    DECL(DRIVING_INCAR_RACER),
    DECL(DRIVING_INCAR_SPORTS),
    DECL(DRIVING_INCAR_LUXURY),
    DECL(DRIVING_FULLGRANDSTAND),
    DECL(DRIVING_EMPTYGRANDSTAND),
    DECL(DRIVING_TUNNEL),

    DECL(CITY_STREETS),
    DECL(CITY_SUBWAY),
    DECL(CITY_MUSEUM),
    DECL(CITY_LIBRARY),
    DECL(CITY_UNDERPASS),
    DECL(CITY_ABANDONED),

    DECL(DUSTYROOM),
    DECL(CHAPEL),
    DECL(SMALLWATERROOM),
};
#undef DECL


int main(int argc, char *argv[])
{
    alure::DeviceManager &devMgr = alure::DeviceManager::get();

    int fileidx = 1;
    alure::Device *dev = [argc,argv,&devMgr,&fileidx]() -> alure::Device*
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
    std::cout<< "Opened \""<<dev->getName()<<"\"" <<std::endl;

    alure::Context *ctx = dev->createContext();
    alure::Context::MakeCurrent(ctx);

    bool gotreverb = false;
    alure::Effect *effect = ctx->createEffect();

    int i = fileidx;
    if(argc-i >= 2 && strcasecmp(argv[i], "-preset") == 0)
    {
        for(size_t j = 0;j < countof(reverblist);j++)
        {
            if(strcasecmp(argv[i+1], reverblist[j].name) == 0)
            {
                std::cout<< "Loading preset "<<reverblist[j].name <<std::endl;
                std::cout.flush();

                effect->setReverbProperties(reverblist[j].props);
                gotreverb = true;
                break;
            }
        }
        i += 2;
    }
    if(!gotreverb)
    {
        std::cout<< "Loading generic preset" <<std::endl;
        std::cout.flush();

        effect->setReverbProperties(EFX_REVERB_PRESET_GENERIC);
    }

    alure::AuxiliaryEffectSlot *auxslot = ctx->createAuxiliaryEffectSlot();
    auxslot->applyEffect(effect);

    for(;i < argc;i++)
    {
        alure::SharedPtr<alure::Decoder> decoder(ctx->createDecoder(argv[i]));
        alure::Source *source = ctx->createSource();

        source->setAuxiliarySend(auxslot, 0);

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
        decoder.reset();
    }

    auxslot->release();
    auxslot = 0;
    effect->destroy();
    effect = 0;

    alure::Context::MakeCurrent(0);
    ctx->destroy();
    ctx = 0;
    dev->close();
    dev = 0;

    return 0;
}
