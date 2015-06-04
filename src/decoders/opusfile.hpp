#ifndef DECODERS_OPUSFILE_HPP
#define DECODERS_OPUSFILE_HPP

#include "alure2.h"

namespace alure {

class OpusFileDecoderFactory : public DecoderFactory {
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_OPUSFILE_HPP */
