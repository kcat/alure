#ifndef DECODERS_WAVE_HPP
#define DECODERS_WAVE_HPP

#include "alure2.h"

namespace alure {

class WaveDecoderFactory : public DecoderFactory {
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_WAVE_HPP */
