#ifndef DECODERS_WAVE_HPP
#define DECODERS_WAVE_HPP

#include "alure2.h"

namespace alure {

class WaveDecoderFactory : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) override final;
};

} // namespace alure

#endif /* DECODERS_WAVE_HPP */
