#ifndef DECODERS_OPUSFILE_HPP
#define DECODERS_OPUSFILE_HPP

#include "alure2.h"

namespace alure {

class OpusFileDecoderFactory final : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) override;
};

} // namespace alure

#endif /* DECODERS_OPUSFILE_HPP */
