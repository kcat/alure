#ifndef DECODERS_SNDFILE_HPP
#define DECODERS_SNDFILE_HPP

#include "alure2.h"

namespace alure {

class SndFileDecoderFactory : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) override final;
};

} // namespace alure

#endif /* DECODERS_SNDFILE_HPP */
