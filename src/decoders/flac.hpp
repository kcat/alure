#ifndef DECODERS_FLAC_HPP
#define DECODERS_FLAC_HPP

#include "alure2.h"

namespace alure {

class FlacDecoderFactory : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) override final;
};

} // namespace alure

#endif /* DECODERS_FLAC_HPP */
