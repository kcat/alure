#ifndef DECODERS_VORBISFILE_HPP
#define DECODERS_VORBISFILE_HPP

#include "alure2.h"

namespace alure {

class VorbisFileDecoderFactory : public DecoderFactory {
    virtual SharedPtr<Decoder> createDecoder(SharedPtr<std::istream> file) final;
};

} // namespace alure

#endif /* DECODERS_VORBISFILE_HPP */
