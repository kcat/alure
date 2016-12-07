#ifndef DECODERS_VORBISFILE_HPP
#define DECODERS_VORBISFILE_HPP

#include "alure2.h"

namespace alure {

class VorbisFileDecoderFactory : public DecoderFactory {
    SharedPtr<Decoder> createDecoder(UniquePtr<std::istream> &file) override final;
};

} // namespace alure

#endif /* DECODERS_VORBISFILE_HPP */
