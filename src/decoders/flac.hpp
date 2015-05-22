#ifndef DECODERS_FLAC_HPP
#define DECODERS_FLAC_HPP

#include "alure2.h"

namespace alure {

class FlacDecoderFactory : public DecoderFactory {
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_FLAC_HPP */
